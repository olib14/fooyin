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

#include <utility>

#include "core/database/librarydatabase.h"
#include "core/models/trackfwd.h"
#include "libraryinfo.h"

#include <utils/worker.h>

class QDir;

namespace Fy::Core {

namespace DB {
class Database;
}

namespace Library {
class LibraryManager;

class LibraryScanner : public Utils::Worker
{
    Q_OBJECT

public:
    explicit LibraryScanner(LibraryInfo* info, DB::Database* database, QObject* parent = nullptr);

    void closeThread() override;
    void stopThread() override;

    void scanLibrary(const TrackPtrList& tracks);

signals:
    void updatedTracks(Core::TrackList tracks);
    void addedTracks(Core::TrackList tracks);
    void tracksDeleted(const Core::TrackPtrList& tracks);

private:
    void storeTracks(TrackList& tracks);
    QStringList getFiles(QDir& baseDirectory);
    bool getAndSaveAllFiles(const TrackPathMap& tracks);

    LibraryInfo* m_library;
    DB::Database* m_database;
    DB::LibraryDatabase m_libraryDatabase;
};
} // namespace Library
} // namespace Fy::Core
