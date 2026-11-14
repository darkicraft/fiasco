#include "fiasco/http/detail/parser.hpp"

#include <llhttp.h>

#include <cstdint>
#include <memory>
#include <string>

#include "fiasco/http/detail/request.hpp"

namespace fiasco::detail {

namespace {

http_method method_from_llhttp(unsigned int method) {
    switch (static_cast<llhttp_method_t>(method)) {
    case HTTP_GET:
        return http_method::get;
    case HTTP_POST:
        return http_method::post;
    case HTTP_PUT:
        return http_method::put;
    case HTTP_DELETE:
        return http_method::del;
    case HTTP_PATCH:
        return http_method::patch;
    case HTTP_HEAD:
        return http_method::head;
    case HTTP_OPTIONS:
        return http_method::options;
    default:
        return http_method::unknown;
    }
}

}  // namespace

struct llhttp_parser::impl {
    struct parser_state {
        request req;
        std::string current_header_field;
        std::string current_header_value;
        bool last_was_value = false;
        bool headers_complete = false;
        bool message_complete = false;
    };

    llhttp_t m_parser{};
    llhttp_settings_t m_settings{};
    parser_state m_state;
    std::string m_error_reason;

    impl() {
        llhttp_settings_init(&m_settings);

        // -- Callbacks --
        m_settings.on_url = [](llhttp_t* p, const char* at, size_t len) {
            auto* s = static_cast<parser_state*>(p->data);
            s->req.url.append(at, len);
            return 0;
        };

        m_settings.on_header_field = [](llhttp_t* p, const char* at, size_t len) {
            auto* s = static_cast<parser_state*>(p->data);
            if (s->last_was_value) {
                // Flush the previous header
                if (!s->current_header_field.empty()) {
                    s->req.headers[s->current_header_field] = s->current_header_value;
                }
                s->current_header_field.clear();
                s->current_header_value.clear();
                s->last_was_value = false;
            }
            s->current_header_field.append(at, len);
            return 0;
        };

        m_settings.on_header_value = [](llhttp_t* p, const char* at, size_t len) {
            auto* s = static_cast<parser_state*>(p->data);
            s->current_header_value.append(at, len);
            s->last_was_value = true;
            return 0;
        };

        m_settings.on_headers_complete = [](llhttp_t* p) {
            auto* s = static_cast<parser_state*>(p->data);
            // Flush last header
            if (!s->current_header_field.empty()) {
                s->req.headers[s->current_header_field] = s->current_header_value;
                s->current_header_field.clear();
                s->current_header_value.clear();
            }

            s->req.method = method_from_llhttp(p->method);
            s->req.keep_alive = llhttp_should_keep_alive(p) != 0;

            // Parse URL into path + query_string (views into req.url)
            auto qpos = s->req.url.find('?');
            if (qpos != std::string::npos) {
                s->req.path = std::string_view(s->req.url.data(), qpos);
                s->req.query_string =
                    std::string_view(s->req.url.data() + qpos + 1, s->req.url.size() - qpos - 1);
            } else {
                s->req.path = std::string_view(s->req.url);
                s->req.query_string = std::string_view();
            }

            // Reserve the body once the length is known. Cap the reservation so
            // a lying Content-Length cannot force a huge upfront allocation.
            constexpr std::uint64_t max_reserve = 1024 * 1024;
            if (p->content_length > 0 && p->content_length <= max_reserve) {
                s->req.body.reserve(static_cast<std::size_t>(p->content_length));
            }

            s->headers_complete = true;
            return 0;
        };

        m_settings.on_body = [](llhttp_t* p, const char* at, size_t len) {
            auto* s = static_cast<parser_state*>(p->data);
            s->req.body.append(at, len);
            return 0;
        };

        m_settings.on_message_complete = [](llhttp_t* p) {
            auto* s = static_cast<parser_state*>(p->data);
            s->message_complete = true;
            return 0;
        };
    }
};

llhttp_parser::llhttp_parser()
    : p_impl(std::make_unique<impl>()) {
    llhttp_init(&p_impl->m_parser, HTTP_REQUEST, &p_impl->m_settings);
    p_impl->m_parser.data = &p_impl->m_state;
    reset();
}

bool llhttp_parser::feed(const char* data, size_t len) {
    auto err = llhttp_execute(&p_impl->m_parser, data, len);
    if (err != HPE_OK && err != HPE_PAUSED) {
        p_impl->m_error_reason = llhttp_errno_name(err);
        return false;
    }
    return true;
}

[[nodiscard]] bool llhttp_parser::is_complete() const noexcept {
    return p_impl->m_state.message_complete;
}

[[nodiscard]] request& llhttp_parser::current() noexcept {
    return p_impl->m_state.req;
}

[[nodiscard]] const request& llhttp_parser::current() const noexcept {
    return p_impl->m_state.req;
}

[[nodiscard]] request llhttp_parser::take_request() {
    request r = std::move(p_impl->m_state.req);
    // Re-anchor path/query_string views into r.url (which now owns the buffer)
    if (!r.url.empty()) {
        auto qpos = r.url.find('?');
        if (qpos != std::string::npos) {
            r.path = std::string_view(r.url.data(), qpos);
            r.query_string = std::string_view(r.url.data() + qpos + 1, r.url.size() - qpos - 1);
        } else {
            r.path = std::string_view(r.url);
            r.query_string = std::string_view();
        }
    }
    reset();
    return r;
}

[[nodiscard]] const std::string& llhttp_parser::error() const noexcept {
    return p_impl->m_error_reason;
}

void llhttp_parser::reset() {
    p_impl->m_state.req.clear();
    p_impl->m_state.current_header_field.clear();
    p_impl->m_state.current_header_value.clear();
    p_impl->m_state.last_was_value = false;
    p_impl->m_state.headers_complete = false;
    p_impl->m_state.message_complete = false;
    p_impl->m_error_reason.clear();
    llhttp_reset(&p_impl->m_parser);
}

llhttp_parser::~llhttp_parser() = default;

}  // namespace fiasco::detail
