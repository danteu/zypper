/*---------------------------------------------------------------------------*\
                          ____  _ _ __ _ __  ___ _ _
                         |_ / || | '_ \ '_ \/ -_) '_|
                         /__|\_, | .__/ .__/\___|_|
                             |__/|_|  |_|
\*---------------------------------------------------------------------------*/

#ifndef ZYPPER_QUERYFORMAT_PARSER_H
#define ZYPPER_QUERYFORMAT_PARSER_H

#include <string_view>

#include "Tokens.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  namespace qf
  {
    bool parse( std::string_view qf_r, Format & result_r );
  } // namespace qf
} // namespace zypp
#endif // ZYPPER_QUERYFORMAT_PARSER_H
