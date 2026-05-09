#include <auracloud/userver/password_hasher.h>

#include <userver/components/component.hpp>
#include <userver/components/component_list.hpp>
#include <userver/formats/json.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/handlers/http_handler_json_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/component.hpp>

#include <string>

namespace auracloud::handlers {

class AuthLogin final : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-auth-login";

  AuthLogin(const userver::components::ComponentConfig& config,
            const userver::components::ComponentContext& context)
      : HttpHandlerJsonBase(config, context),
        pg_(context.FindComponent<userver::components::Postgres>("auracloud-db")
                .GetCluster()) {}

  Value HandleRequestJsonThrow(
      const userver::server::http::HttpRequest&,
      const userver::formats::json::Value& request_json,
      userver::server::request::RequestContext&) const override {
    const auto email = request_json["email"].As<std::string>("");
    const auto password = request_json["password"].As<std::string>("");
    if (email.empty() || password.empty()) {
      userver::formats::json::ValueBuilder vb;
      vb["ok"] = false;
      vb["code"] = "invalid_input";
      vb["message"] = "email/password required";
      return vb.ExtractValue();
    }

    const auto res = pg_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "SELECT salt, password_hash FROM auracloud.users WHERE email=$1", email);
    if (res.IsEmpty()) {
      userver::formats::json::ValueBuilder vb;
      vb["ok"] = false;
      vb["code"] = "not_found";
      vb["message"] = "user not found";
      return vb.ExtractValue();
    }

    const auto row = res.Front();
    const auto salt = row["salt"].As<std::string>();
    const auto want = row["password_hash"].As<std::string>();
    const auto got = auracloud::security::HashPasswordSha3_256(password, salt).hash;
    if (got != want) {
      userver::formats::json::ValueBuilder vb;
      vb["ok"] = false;
      vb["code"] = "unauthorized";
      vb["message"] = "invalid credentials";
      return vb.ExtractValue();
    }

    // MVP token (not JWT yet).
    const std::string token =
        std::to_string(std::hash<std::string>{}(email + ":" + want));

    userver::formats::json::ValueBuilder vb;
    vb["ok"] = true;
    vb["token"] = token;
    return vb.ExtractValue();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_;
};

}  // namespace auracloud::handlers

void AppendAuthLogin(userver::components::ComponentList& list) {
  list.Append<auracloud::handlers::AuthLogin>();
}

