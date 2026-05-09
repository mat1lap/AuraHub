#include <userver/components/component.hpp>
#include <userver/components/component_list.hpp>
#include <userver/formats/json.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/handlers/http_handler_json_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

#include <string>

namespace auracloud::handlers {

class Settings final : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-settings";

  Settings(const userver::components::ComponentConfig& config,
           const userver::components::ComponentContext& context)
      : HttpHandlerJsonBase(config, context),
        pg_(context.FindComponent<userver::components::Postgres>("auracloud-db")
                .GetCluster()) {}

  Value HandleRequestJsonThrow(
      const userver::server::http::HttpRequest& request,
      const userver::formats::json::Value& request_json,
      userver::server::request::RequestContext&) const override {
    const auto user = request.GetHeader("X-Aura-User");
    if (user.empty()) {
      userver::formats::json::ValueBuilder vb;
      vb["ok"] = false;
      vb["code"] = "unauthorized";
      vb["message"] = "missing X-Aura-User header (MVP auth)";
      return vb.ExtractValue();
    }

    if (request.GetMethod() == userver::server::http::HttpMethod::kGet) {
      const auto res = pg_->Execute(
          userver::storages::postgres::ClusterHostType::kMaster,
          // JSONB columns: read as text for stable userver/pg mapping, then parse JSON.
          "SELECT system_prompts::text, mcp_presets::text FROM auracloud.user_settings "
          "WHERE email=$1",
          user);
      userver::formats::json::ValueBuilder vb;
      vb["ok"] = true;
      if (res.IsEmpty()) {
        vb["system_prompts"] = userver::formats::json::ValueBuilder(
            userver::formats::json::Type::kArray);
        vb["mcp_presets"] = userver::formats::json::ValueBuilder(
            userver::formats::json::Type::kObject);
      } else {
        const auto row = res.Front();
        vb["system_prompts"] =
            userver::formats::json::FromString(row["system_prompts"].As<std::string>());
        vb["mcp_presets"] =
            userver::formats::json::FromString(row["mcp_presets"].As<std::string>());
      }
      return vb.ExtractValue();
    }

    if (request.GetMethod() == userver::server::http::HttpMethod::kPut) {
      const auto system_prompts =
          request_json["system_prompts"].As<std::string>("[]");
      const auto mcp_presets = request_json["mcp_presets"].As<std::string>("{}");

      pg_->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                   "INSERT INTO auracloud.user_settings(email, system_prompts, "
                   "mcp_presets) VALUES($1, $2::jsonb, $3::jsonb) "
                   "ON CONFLICT(email) DO UPDATE SET system_prompts=$2::jsonb, "
                   "mcp_presets=$3::jsonb, updated_at=now()",
                   user, system_prompts, mcp_presets);

      userver::formats::json::ValueBuilder vb;
      vb["ok"] = true;
      return vb.ExtractValue();
    }

    userver::formats::json::ValueBuilder vb;
    vb["ok"] = false;
    vb["code"] = "method_not_allowed";
    return vb.ExtractValue();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_;
};

}  // namespace auracloud::handlers

void AppendSettings(userver::components::ComponentList& list) {
  list.Append<auracloud::handlers::Settings>();
}

