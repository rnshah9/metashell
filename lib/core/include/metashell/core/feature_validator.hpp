#ifndef METASHELL_FEATURE_VALIDATOR_HPP
#define METASHELL_FEATURE_VALIDATOR_HPP

// Metashell - Interactive C++ template metaprogramming shell
// Copyright (C) 2017, Abel Sinkovics (abel@sinkovics.hu)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <metashell/data/feature.hpp>
#include <metashell/data/real_engine_name.hpp>

#include <metashell/core/not_supported.hpp>

#include <type_traits>
#include <vector>

namespace metashell
{
  namespace core
  {
    class feature_validator
    {
    public:
      feature_validator(data::real_engine_name engine_,
                        std::vector<data::feature> supported_features_);

      template <class FeatureUsed>
      feature_validator& check(data::feature feature_, const FeatureUsed&)
      {
        check(feature_, !std::is_same<FeatureUsed, not_supported>::value);

        return *this;
      }

      void all_checked();

    private:
      data::real_engine_name _engine;
      std::vector<data::feature> _supported_features;
      bool _all_checked;

      void check(data::feature feature_, bool really_supported_);
    };
  } // namespace core
} // namespace metashell

#endif
