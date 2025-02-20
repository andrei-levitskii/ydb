#pragma once

#include <yql/essentials/ast/yql_expr.h>
#include <yql/essentials/ast/yql_pos_handle.h>
#include <yql/essentials/minikql/mkql_type_builder.h>

namespace NKikimr {
namespace NMiniKQL {

class TType;
class TStructType;
class TProgramBuilder;

}
}

namespace NYql {
namespace NCommon {

NKikimr::NMiniKQL::TType* BuildType(const TTypeAnnotationNode& annotation, const NKikimr::NMiniKQL::TTypeBuilder& typeBuilder, IOutputStream& err);
NKikimr::NMiniKQL::TType* BuildType(TPositionHandle pos, const TTypeAnnotationNode& annotation, NKikimr::NMiniKQL::TTypeBuilder& typeBuilder);
NKikimr::NMiniKQL::TType* BuildType(const TExprNode& owner, const TTypeAnnotationNode& annotation, NKikimr::NMiniKQL::TTypeBuilder& typeBuilder);

const TTypeAnnotationNode* ConvertMiniKQLType(TPosition position, NKikimr::NMiniKQL::TType* type, TExprContext& ctx);
ETypeAnnotationKind ConvertMiniKQLTypeKind(NKikimr::NMiniKQL::TType* type);

} // namespace NCommon
} // namespace NYql
