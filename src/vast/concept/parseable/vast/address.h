#ifndef VAST_CONCEPT_PARSEABLE_VAST_ADDRESS_H
#define VAST_CONCEPT_PARSEABLE_VAST_ADDRESS_H

#include <arpa/inet.h>  // inet_pton
#include <sys/socket.h> // AF_INET*

#include "vast/address.h"

#include "vast/concept/parseable/core.h"
#include "vast/concept/parseable/numeric/integral.h"
#include "vast/concept/parseable/string/char_class.h"

namespace vast {

struct address_parser : vast::parser<address_parser>
{
  static auto make_v4()
  {
    using namespace parsers;
    auto dec
      = integral_parser<uint16_t, 3, 1>{}.with([](auto i) { return i < 256; });
      ;
    auto v4
      = dec >> '.' >> dec >> '.' >> dec >> '.' >> dec
      ;
    return v4;
  }

  static auto make_v6()
  {
    using namespace parsers;
    auto h16
      = rep<1, 4>(xdigit)
      ;
    // Matches a 1-4 hex digits followed by a *single* colon. If we did not have
    // this parser, the input "f00::" would not be detected correctly, since a
    // rule of the form
    //
    //    -(repeat<0, *>{h16 >> ':'} >> h16) >> "::"
    //
    // already consumes the input "f00:" after the first repitition parser, thus
    // erroneously leaving only ":" for next rule `>> h16` to consume.
    auto h16_colon
      = h16 >> ':' >> !lit(':')
      ;
    auto ls32
      = h16 >> ':' >> h16
      | make_v4();
      ;
    auto v6
      =                                             rep<6>(h16 >> ':') >> ls32
      |                                     "::" >> rep<5>(h16 >> ':') >> ls32
      |   -(                        h16) >> "::" >> rep<4>(h16 >> ':') >> ls32
      |   -(rep<0, 1>(h16_colon) >> h16) >> "::" >> rep<3>(h16 >> ':') >> ls32
      |   -(rep<0, 2>(h16_colon) >> h16) >> "::" >> rep<2>(h16 >> ':') >> ls32
      |   -(rep<0, 3>(h16_colon) >> h16) >> "::" >>        h16 >> ':'  >> ls32
      |   -(rep<0, 4>(h16_colon) >> h16) >> "::"                       >> ls32
      |   -(rep<0, 5>(h16_colon) >> h16) >> "::"                       >> h16
      |   -(rep<0, 6>(h16_colon) >> h16) >> "::"
      ;
    return v6;
  }

  template <typename Iterator>
  bool parse(Iterator& f, Iterator const& l, unused_type) const
  {
    static auto v4 = make_v4();
    static auto v6 = make_v6();
    if (v4.parse(f, l, unused))
      return true;
    if (v6.parse(f, l, unused))
      return true;
    return false;
  }
};


template <>
struct access::parser<address> : vast::parser<access::parser<address>>
{
  template <typename Iterator>
  bool parse(Iterator& f, Iterator const& l, unused_type) const
  {
    static auto const p = address_parser{};
    return p.parse(f, l, unused);
  }

  template <typename Iterator>
  bool parse(Iterator& f, Iterator const& l, address& a) const
  {
    static auto const v4 = address_parser::make_v4();
    auto a4 = std::tie(a.bytes_[12], a.bytes_[13], a.bytes_[14], a.bytes_[15]);
    auto begin = f;
    if (v4.parse(f, l, a4))
    {
      std::copy(address::v4_mapped_prefix.begin(),
                address::v4_mapped_prefix.end(),
                a.bytes_.begin());
      return true;
    }
    static auto const v6 = address_parser::make_v6();
    if (v6.parse(f, l, unused))
      // We still need to enhance the parseable concept with a few more tools
      // so that we can transparently parse into 16-byte sequence. Until
      // then, we rely on ::inet_pton.
      // FIXME: Using &*Iterator only works for contiguous iterators, which
      // most likely encounter in practice. We really should use something like
      // N4284 as soon as it's available.
      return ::inet_pton(AF_INET6, &*begin, &a.bytes_) == 1;
    return false;
  }
};

template <>
struct parser_registry<address>
{
  using type = access::parser<address>;
};

} // namespace vast

#endif