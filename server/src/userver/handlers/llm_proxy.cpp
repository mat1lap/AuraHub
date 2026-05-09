#include <userver/components/component.hpp>
#include <userver/components/component_list.hpp>
#include <userver/formats/json.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/handlers/http_handler_json_base.hpp>

#include <string>

namespace auracloud::handlers {

class LlmProxy final : public userver::server::handlers::HttpHandlerJsonBase {
 public:
  static constexpr std::string_view kName = "handler-llm-proxy";

  using HttpHandlerJsonBase::HttpHandlerJsonBase;

  Value HandleRequestJsonThrow(
      const userver::server::http::HttpRequest&,
      const userver::formats::json::Value& request_json,
      userver::server::request::RequestContext&) const override {
    // Minimal stub: returns the prompt back. Real implementation will:
    // - validate auth (JWT)
    // - inject provider keys from server secrets
    // - call provider over HTTPS
    // - return model response
    userver::formats::json::ValueBuilder vb;
    vb["ok"] = true;
    vb["echo"] = request_json;
    vb["warning"] = "LLM proxy not implemented yet";
    return vb.ExtractValue();
  }
};

}  // namespace auracloud::handlers

void AppendLlmProxy(userver::components::ComponentList& list) {
  list.Append<auracloud::handlers::LlmProxy>();
}

