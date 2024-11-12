LIBRARY()

SRCS(
    manager.h
    manager.cpp
)

STYLE_CPP()

PEERDIR(
    ydb/library/actors/core
    ydb/library/services
    ydb/library/yql/providers/common/metrics
    ydb/library/yql/utils
)

END()
