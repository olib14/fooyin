/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fooyin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include "filterfwd.h"

#include <utils/helpers.h>

namespace Fy::Filters {
class FilterStore
{
public:
    [[nodiscard]] FilterList filters() const;

    // TODO: Allow user-defined types and indexes
    LibraryFilter& addFilter(FilterType type);
    void removeFilter(FilterType type);

    LibraryFilter* find(FilterType type);
    [[nodiscard]] bool hasFilter(FilterType type) const;

    [[nodiscard]] bool filterIsActive(FilterType type) const;
    [[nodiscard]] bool hasActiveFilters() const;
    [[nodiscard]] FilterList activeFilters() const;
    void deactivateFilter(FilterType type);

private:
    FilterList m_filters;
};
} // namespace Fy::Filters
