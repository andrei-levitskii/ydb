IF (HOST_OS_DARWIN AND HOST_ARCH_X86_64)
    DECLARE_EXTERNAL_RESOURCE(TEST_TOOL_HOST sbr:7480256374)
ELSEIF (HOST_OS_DARWIN AND HOST_ARCH_ARM64)
    DECLARE_EXTERNAL_RESOURCE(TEST_TOOL_HOST sbr:7480255951)
ELSEIF (HOST_OS_LINUX AND HOST_ARCH_X86_64)
    DECLARE_EXTERNAL_RESOURCE(TEST_TOOL_HOST sbr:7480257071)
ELSEIF (HOST_OS_LINUX AND HOST_ARCH_AARCH64)
    DECLARE_EXTERNAL_RESOURCE(TEST_TOOL_HOST sbr:7480255328)
ELSEIF (HOST_OS_WINDOWS AND HOST_ARCH_X86_64)
    DECLARE_EXTERNAL_RESOURCE(TEST_TOOL_HOST sbr:7480256738)

ENDIF()
