option (ENABLE_RDRAND "Use RDRAND hardware random number generation, if available" ON)

if (ENABLE_RDRAND)
    set(QRACK_COMPILE_OPTS ${QRACK_COMPILE_OPTS} -mrdrnd)
    target_compile_definitions(qrack PUBLIC ENABLE_RDRAND=1)
endif (ENABLE_RDRAND)

message ("Try RDRAND is: ${ENABLE_RDRAND}")

if (ENABLE_ANURAND)
    target_compile_definitions(qrack PUBLIC ENABLE_ANURAND=1)
    target_link_libraries (qrack PocoNet PocoUtil PocoFoundation PocoNetSSL)
endif (ENABLE_ANURAND)

message ("Enable ANURAND is: ${ENABLE_ANURAND}")
