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

#include "core/app/worker.h"
#include "core/models/trackfwd.h"
#include "libraryinfo.h"

class QDir;

namespace Core {

namespace DB {
class Database;
}

namespace Library {
class LibraryManager;

class LibraryScanner : public Worker
{
    Q_OBJECT

public:
    explicit LibraryScanner(LibraryManager* libraryManager, DB::Database* database, QObject* parent = nullptr);
    ~LibraryScanner() override;

    void stopThread() override;

    void scanLibrary(const TrackList tracks, const LibraryInfo& info);
    void scanAll(const TrackList tracks);

signals:
    void libraryAdded();
    void updatedTracks(Core::TrackList tracks);
    void addedTracks(Core::TrackList tracks);
    void tracksDeleted(const Core::IdSet& tracks);

protected:
    void storeTracks(TrackList& tracks) const;
    QStringList getFiles(QDir& baseDirectory);
    bool getAndSaveAllFiles(int libraryId, const QString& path, const TrackPathMap& tracks);

private:
    LibraryManager* m_libraryManager;
    DB::Database* m_database;
};
} // namespace Library
} // namespace Core