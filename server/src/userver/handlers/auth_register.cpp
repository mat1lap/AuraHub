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

class AuthRegister final : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-auth-register";

  AuthRegister(const userver::components::ComponentConfig& config,
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

    const auto salt = std::to_string(std::hash<std::string>{}(email));
    const auto ph = auracloud::security::HashPasswordSha3_256(password, salt);
    if (ph.hash.empty()) {
      userver::formats::json::ValueBuilder vb;
      vb["ok"] = false;
      vb["code"] = "hash_failed";
      vb["message"] = "password hash failed";
      return vb.ExtractValue();
    }

    // Insert; on conflict => already exists.
    const auto res = pg_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO auracloud.users(email, salt, password_hash) "
        "VALUES($1, $2, $3) "
        "ON CONFLICT (email) DO NOTHING",
        email, ph.salt, ph.hash);

    userver::formats::json::ValueBuilder vb;
    if (res.RowsAffected() == 0) {
      vb["ok"] = false;
      vb["code"] = "already_exists";
      vb["message"] = "user already exists";
      return vb.ExtractValue();
    }
    vb["ok"] = true;
    return vb.ExtractValue();
  }

 private:
  userver::storages::postgres::ClusterPtr pg_;
};

}  // namespace auracloud::handlers

void AppendAuthRegister(userver::components::ComponentList& list) {
  list.Append<auracloud::handlers::AuthRegister>();
}

