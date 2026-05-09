add_executable(auracloud
  src/main.cpp
  src/services/auth_service.cpp
  src/services/sync_service.cpp
  src/services/llm_gateway.cpp
)

target_include_directories(auracloud PRIVATE include)
target_link_libraries(auracloud 
  PRIVATE
  nlohmann_json::nlohmann_json
  aurahub_warnings
)

find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
  target_compile_definitions(auracloud PRIVATE AURAHUB_HAVE_OPENSSL=1)
  target_link_libraries(auracloud PRIVATE OpenSSL::Crypto)
endif()

# AuraCloud (final target): userver + PostgreSQL.
# This is built when userver is available (either installed or fetched).
option(AURAHUB_BUILD_USERVER "Build AuraCloud userver service" OFF)

if(AURAHUB_BUILD_USERVER)
  find_program(PKG_CONFIG_EXECUTABLE NAMES pkg-config pkgconf)
  if(NOT PKG_CONFIG_EXECUTABLE)
    message(WARNING "pkg-config/pkgconf not found; skipping auracloud_userver build")
    set(AURAHUB_BUILD_USERVER OFF)
  endif()
endif()

if(AURAHUB_BUILD_USERVER)
  include(FetchContent)
  # Prefer installed userver if available.
  find_package(userver QUIET CONFIG)
  if(NOT userver_FOUND)
    # userver defaults can disable some features depending on deps; we force
    # the minimal feature set we need for AuraCloud.
    set(USERVER_FEATURE_POSTGRESQL ON CACHE BOOL "" FORCE)
    set(USERVER_FEATURE_STACKTRACE OFF CACHE BOOL "" FORCE)
    set(USERVER_FEATURE_TESTSUITE OFF CACHE BOOL "" FORCE)
    set(USERVER_FEATURE_UTEST OFF CACHE BOOL "" FORCE)
    set(USERVER_FEATURE_JEMALLOC OFF CACHE BOOL "" FORCE)
    set(USERVER_FEATURE_PATCH_LIBPQ OFF CACHE BOOL "" FORCE)
    set(USERVER_CHECK_PACKAGE_VERSIONS 0 CACHE BOOL "" FORCE)

    FetchContent_Declare(
      userver
      GIT_REPOSITORY https://github.com/userver-framework/userver.git
      # NOTE: pinned for reproducibility. Update when needed.
      GIT_TAG v2.14
    )
    FetchContent_MakeAvailable(userver)
  endif()

  add_executable(auracloud_userver
    src/userver/main.cpp
    src/userver/handlers/auth_register.cpp
    src/userver/handlers/auth_login.cpp
    src/userver/handlers/settings.cpp
    src/userver/handlers/llm_proxy.cpp
    src/userver/password_hasher.cpp
  )

  target_include_directories(auracloud_userver PRIVATE include)
  target_link_libraries(auracloud_userver PRIVATE
    aurahub_warnings
    # Depending on whether userver is installed or fetched, targets may be
    # namespaced (userver::core) or plain (userver-core).
  )

  if(TARGET userver::core)
    target_link_libraries(auracloud_userver PRIVATE userver::core)
  elseif(TARGET userver-core)
    target_link_libraries(auracloud_userver PRIVATE userver-core)
  endif()

  if(TARGET userver::postgresql)
    target_link_libraries(auracloud_userver PRIVATE userver::postgresql)
  elseif(TARGET userver-postgresql)
    target_link_libraries(auracloud_userver PRIVATE userver-postgresql)
  endif()

  # Some userver builds expose universal as a separate target.
  if(TARGET userver::universal)
    target_link_libraries(auracloud_userver PRIVATE userver::universal)
  elseif(TARGET userver-universal)
    target_link_libraries(auracloud_userver PRIVATE userver-universal)
  endif()

  # When userver is built as static libs, some transitive deps may not propagate.
  if(TARGET cryptopp)
    target_link_libraries(auracloud_userver PRIVATE cryptopp)
  endif()
  if(TARGET cctz)
    target_link_libraries(auracloud_userver PRIVATE cctz)
  endif()

  if(OpenSSL_FOUND)
    target_compile_definitions(auracloud_userver PRIVATE AURAHUB_HAVE_OPENSSL=1)
    target_link_libraries(auracloud_userver PRIVATE OpenSSL::Crypto)
  endif()
endif()

