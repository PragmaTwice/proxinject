#include <protopuf/message.h>

using pp::operator""_f;

using InjecteeConnect =
    pp::message<pp::uint32_field<"handle", 1>, pp::string_field<"addr", 2>>;

using InjecteeMessage =
    pp::message<pp::string_field<"opcode", 1>,
                pp::message_field<"connect", 2, InjecteeConnect>,
                pp::uint32_field<"pid", 3>>;

using InjectorMessage = pp::message<pp::string_field<"opcode", 1>>;

template <typename M, pp::basic_fixed_string S, typename T> M create_message(T&& v) {
  M msg;

  msg["opcode"_f] = S.data;
  msg.get<S>() = std::forward<T>(v);

  return msg;
}
