#include "helpers/get_value.h"
#include "helpers/local.h"
#include "helpers/query_executor.h"
#include "helpers/typed_local.h"
#include "helpers/writer.h"

#include <ydb/core/kqp/ut/common/columnshard.h>
#include <ydb/core/tx/columnshard/engines/scheme/abstract/index_info.h>
#include <ydb/core/tx/columnshard/hooks/testing/controller.h>
#include <ydb/core/tx/columnshard/test_helper/controllers.h>
#include <ydb/core/wrappers/fake_storage.h>

namespace NKikimr::NKqp {

Y_UNIT_TEST_SUITE(KqpOlapTiering) {
    Y_UNIT_TEST(Eviction) {
        auto csController = NYDBTest::TControllers::RegisterCSControllerGuard<NOlap::TWaitCompactionController>();
        csController->SetSkipSpecialCheckForEvict(true);

        TKikimrSettings runnerSettings;
        runnerSettings.WithSampleTables = false;
        TTestHelper testHelper(runnerSettings);
        TLocalHelper localHelper(testHelper.GetKikimr());
        NYdb::NTable::TTableClient tableClient = testHelper.GetKikimr().GetTableClient();
        Tests::NCommon::TLoggerInit(testHelper.GetKikimr()).Initialize();
        Singleton<NKikimr::NWrappers::NExternalStorage::TFakeExternalStorage>()->SetSecretKey("fakeSecret");

        localHelper.CreateTestOlapTable();
        testHelper.CreateTier("tier1");
        const TString tieringRule = testHelper.CreateTieringRule("tier1", "timestamp");

        for (ui64 i = 0; i < 100; ++i) {
            WriteTestData(testHelper.GetKikimr(), "/Root/olapStore/olapTable", 0, i * 10000, 1000);
            WriteTestData(testHelper.GetKikimr(), "/Root/olapStore/olapTable", 0, i * 10000, 1000);
        }

        csController->WaitCompactions(TDuration::Seconds(5));
        csController->WaitActualization(TDuration::Seconds(5));

        ui64 columnRawBytes = 0;
        {
            auto selectQuery = TString(R"(
                SELECT
                    TierName, SUM(ColumnRawBytes) As RawBytes
                FROM `/Root/olapStore/olapTable/.sys/primary_index_portion_stats`
                WHERE Activity == 1
                GROUP BY TierName
            )");

            auto rows = ExecuteScanQuery(tableClient, selectQuery);
            UNIT_ASSERT_VALUES_EQUAL(rows.size(), 1);
            UNIT_ASSERT_VALUES_EQUAL(GetUtf8(rows[0].at("TierName")), "__DEFAULT");

            columnRawBytes = GetUint64(rows[0].at("RawBytes"));
            UNIT_ASSERT_GT(columnRawBytes, 0);
        }

        testHelper.SetTiering("/Root/olapStore/olapTable", tieringRule);
        csController->WaitActualization(TDuration::Seconds(5));

        {
            auto selectQuery = TString(R"(
                SELECT
                    TierName, SUM(ColumnRawBytes) As RawBytes
                FROM `/Root/olapStore/olapTable/.sys/primary_index_portion_stats`
                WHERE Activity == 1
                GROUP BY TierName
            )");

            auto rows = ExecuteScanQuery(tableClient, selectQuery);
            UNIT_ASSERT_VALUES_EQUAL(rows.size(), 1);
            UNIT_ASSERT_VALUES_EQUAL(GetUtf8(rows[0].at("TierName")), "tier1");
            UNIT_ASSERT_VALUES_EQUAL_C(GetUint64(rows[0].at("RawBytes")), columnRawBytes,
                TStringBuilder() << "RawBytes changed after eviction: before=" << columnRawBytes
                                 << " after=" << GetUint64(rows[0].at("RawBytes")));
        }

        testHelper.ResetTiering("/Root/olapStore/olapTable");
        csController->WaitCompactions(TDuration::Seconds(5));

        {
            auto selectQuery = TString(R"(
                SELECT
                    TierName, SUM(ColumnRawBytes) As RawBytes
                FROM `/Root/olapStore/olapTable/.sys/primary_index_portion_stats`
                WHERE Activity == 1
                GROUP BY TierName
            )");

            auto rows = ExecuteScanQuery(tableClient, selectQuery);
            UNIT_ASSERT_VALUES_EQUAL(rows.size(), 1);
            UNIT_ASSERT_VALUES_EQUAL(GetUtf8(rows[0].at("TierName")), "__DEFAULT");
            UNIT_ASSERT_VALUES_EQUAL_C(GetUint64(rows[0].at("RawBytes")), columnRawBytes,
                TStringBuilder() << "RawBytes changed after resetting tiering: before=" << columnRawBytes
                                 << " after=" << GetUint64(rows[0].at("RawBytes")));
        }
    }

    Y_UNIT_TEST(TieringRuleValidation) {
        auto csController = NYDBTest::TControllers::RegisterCSControllerGuard<NOlap::TWaitCompactionController>();

        TKikimrSettings runnerSettings;
        runnerSettings.WithSampleTables = false;
        TTestHelper testHelper(runnerSettings);
        TLocalHelper localHelper(testHelper.GetKikimr());
        NYdb::NTable::TTableClient tableClient = testHelper.GetKikimr().GetTableClient();
        Tests::NCommon::TLoggerInit(testHelper.GetKikimr()).Initialize();
        Singleton<NKikimr::NWrappers::NExternalStorage::TFakeExternalStorage>()->SetSecretKey("fakeSecret");

        localHelper.CreateTestOlapTable();
        testHelper.CreateTier("tier1");

        {
            const TString query = R"(
            CREATE OBJECT IF NOT EXISTS empty_tiering_rule (TYPE TIERING_RULE)
                WITH (defaultColumn = timestamp, description = `{"rules": []}`))";
            auto result = testHelper.GetSession().ExecuteSchemeQuery(query).GetValueSync();
            UNIT_ASSERT_VALUES_UNEQUAL(result.GetStatus(), NYdb::EStatus::SUCCESS);
        }

        {
            const TString query = R"(
            CREATE OBJECT IF NOT EXISTS empty_default_column (TYPE TIERING_RULE)
                WITH (defaultColumn = ``, description = `{"rules": [{ "tierName" : "tier1", "durationForEvict" : "10d" }]}`))";
            auto result = testHelper.GetSession().ExecuteSchemeQuery(query).GetValueSync();
            UNIT_ASSERT_VALUES_UNEQUAL(result.GetStatus(), NYdb::EStatus::SUCCESS);
        }

        {
            const TString query = R"(
            CREATE OBJECT IF NOT EXISTS no_default_column (TYPE TIERING_RULE)
                WITH (description = `{"rules": [{ "tierName" : "tier1", "durationForEvict" : "10d" }]}`))";
            auto result = testHelper.GetSession().ExecuteSchemeQuery(query).GetValueSync();
            UNIT_ASSERT_VALUES_UNEQUAL(result.GetStatus(), NYdb::EStatus::SUCCESS);
        }

        const TString correctTieringRule = testHelper.CreateTieringRule("tier1", "timestamp");
        {
            const TString query = "ALTER OBJECT " + correctTieringRule + R"( (TYPE TIERING_RULE) SET description `{"rules": []}`)";
            auto result = testHelper.GetSession().ExecuteSchemeQuery(query).GetValueSync();
            UNIT_ASSERT_VALUES_UNEQUAL(result.GetStatus(), NYdb::EStatus::SUCCESS);
        }

        {
            const TString query = "ALTER OBJECT " + correctTieringRule + R"( (TYPE TIERING_RULE) SET description `{"rules": []}`)";
            auto result = testHelper.GetSession().ExecuteSchemeQuery(query).GetValueSync();
            UNIT_ASSERT_VALUES_UNEQUAL(result.GetStatus(), NYdb::EStatus::SUCCESS);
        }

        {
            const TString query = "ALTER OBJECT " + correctTieringRule + R"( (TYPE TIERING_RULE) SET defaultColumn ``)";
            auto result = testHelper.GetSession().ExecuteSchemeQuery(query).GetValueSync();
            UNIT_ASSERT_VALUES_UNEQUAL(result.GetStatus(), NYdb::EStatus::SUCCESS);
        }

        {
            const TString query = "ALTER OBJECT " + correctTieringRule + R"( (TYPE TIERING_RULE) RESET defaultColumn)";
            auto result = testHelper.GetSession().ExecuteSchemeQuery(query).GetValueSync();
            UNIT_ASSERT_VALUES_UNEQUAL(result.GetStatus(), NYdb::EStatus::SUCCESS);
        }
    }
}

}   // namespace NKikimr::NKqp
