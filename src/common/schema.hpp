#include <protopuf/message.h>

using pp::operator""_f;

using IpAddr =
    pp::message<pp::uint32_field<"v4_addr", 1>, pp::bytes_field<"v6_addr", 2>,
                pp::uint32_field<"port", 3>>;

std::pair<ip::address, std::uint16_t> to_asio(const IpAddr &ip) {
  if (const auto &v = ip["v4_addr"_f]) {
    return {ip::address_v4(v.value()), ip["port"_f].value()};
  } else {
    ip::address_v6::bytes_type addr;
    const auto &origin = ip["v6_addr"_f].value();
    std::copy(origin.begin(), origin.end(), addr.begin());
    return {ip::address_v6(addr), ip["port"_f].value()};
  }
}

using InjecteeConnect = pp::message<pp::uint32_field<"handle", 1>,
                                    pp::message_field<"addr", 2, IpAddr>>;

using InjecteeMessage =
    pp::message<pp::string_field<"opcode", 1>,
                pp::message_field<"connect", 2, InjecteeConnect>,
                pp::uint32_field<"pid", 3>>;

using InjectorConfig = pp::message<pp::message_field<"addr", 1, IpAddr>>;

using InjectorMessage =
    pp::message<pp::string_field<"opcode", 1>,
                pp::message_field<"config", 2, InjectorConfig>>;

template <typename M, pp::basic_fixed_string S, typename T>
M create_message(T &&v) {
  M msg;

  msg["opcode"_f] = S.data;
  msg.get<S>() = std::forward<T>(v);

  return msg;
}

template <pp::basic_fixed_string S, typename M>
M::template get_type_by_name<S>::base_type compare_message(const M &msg) {
  if (msg["opcode"_f] == S.data) {
    return msg.get_base<S>();
  }

  return std::nullopt;
}
