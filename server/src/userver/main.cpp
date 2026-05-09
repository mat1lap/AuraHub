#include <userver/clients/dns/component.hpp>
#include <userver/components/component_list.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/utils/daemon_run.hpp>

// Handler appenders (declared in .cpp translation units).
void AppendAuthRegister(userver::components::ComponentList& list);
void AppendAuthLogin(userver::components::ComponentList& list);
void AppendSettings(userver::components::ComponentList& list);
void AppendLlmProxy(userver::components::ComponentList& list);

int main(int argc, const char* const argv[]) {
  auto list = userver::components::MinimalServerComponentList();

  // PostgreSQL driver expects TestsuiteSupport + DNS client (same as userver postgres samples).
  list.Append<userver::components::TestsuiteSupport>();
  list.Append<userver::clients::dns::Component>();

  // PostgreSQL component (static config name: auracloud-db).
  list.Append<userver::components::Postgres>("auracloud-db");

  AppendAuthRegister(list);
  AppendAuthLogin(list);
  AppendSettings(list);
  AppendLlmProxy(list);

  return userver::utils::DaemonMain(argc, argv, list);
}

