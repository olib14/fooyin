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

#include "playlistmodel.h"

#include "gui/guiconstants.h"
#include "gui/guisettings.h"
#include "playlistitem.h"

#include <core/library/coverprovider.h>
#include <core/player/playermanager.h>
#include <core/playlist/playlisthandler.h>

#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QIcon>
#include <QPalette>

namespace Fy::Gui::Widgets {
QString trackArtistString(const Core::Track& track)
{
    QString artistString;
    for(const QString& artist : track.artists()) {
        if(artist != track.albumArtist()) {
            if(!artistString.isEmpty()) {
                artistString += ", ";
            }
            artistString += artist;
        }
    }
    if(!artistString.isEmpty()) {
        artistString.prepend("  \u2022  ");
    }
    return artistString;
}

PlaylistModel::PlaylistModel(Core::Player::PlayerManager* playerManager,
                             Core::Playlist::PlaylistHandler* playlistHandler, Utils::SettingsManager* settings,
                             QObject* parent)
    : TreeModel{parent}
    , m_playerManager{playerManager}
    , m_playlistHandler{playlistHandler}
    , m_settings{settings}
    , m_discHeaders{m_settings->value<Settings::DiscHeaders>()}
    , m_splitDiscs{m_settings->value<Settings::SplitDiscs>()}
    , m_altColours{m_settings->value<Settings::PlaylistAltColours>()}
    , m_simplePlaylist{m_settings->value<Settings::SimplePlaylist>()}
    , m_resetting{false}
    , m_playingIcon{QIcon::fromTheme(Constants::Icons::Play).pixmap(20)}
    , m_pausedIcon{QIcon::fromTheme(Constants::Icons::Pause).pixmap(20)}
{
    setupModelData();

    m_settings->subscribe<Settings::DiscHeaders>(this, [this](bool enabled) {
        m_discHeaders = enabled;
        reset();
    });
    m_settings->subscribe<Settings::SplitDiscs>(this, [this](bool enabled) {
        m_splitDiscs = enabled;
        reset();
    });
    m_settings->subscribe<Settings::PlaylistAltColours>(this, [this](bool enabled) {
        m_altColours = enabled;
        emit dataChanged({}, {}, {Qt::BackgroundRole});
    });
    m_settings->subscribe<Settings::SimplePlaylist>(this, [this](bool enabled) {
        m_simplePlaylist = enabled;
        reset();
    });
}

QVariant PlaylistModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    const auto playlist = m_playlistHandler->activePlaylist();
    if(!playlist) {
        return {};
    }
    return QString("%1: %2 Tracks").arg(playlist->name()).arg(playlist->trackCount());
}

QVariant PlaylistModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid()) {
        return {};
    }

    auto* item = static_cast<PlaylistItem*>(index.internalPointer());

    const PlaylistItem::Type type = item->type();

    if(role == Playlist::Mode) {
        return m_simplePlaylist;
    }

    if(role == Playlist::Type) {
        return QVariant::fromValue<PlaylistItem::Type>(type);
    }

    switch(type) {
        case(PlaylistItem::Album):
            return albumData(item, role);
        case(PlaylistItem::Track):
            return trackData(item, role);
        case(PlaylistItem::Container):
            return containerData(item, role);
        case(PlaylistItem::Root):
            return {};
    }
    return {};
}

QHash<int, QByteArray> PlaylistModel::roleNames() const
{
    auto roles = QAbstractItemModel::roleNames();

    roles.insert(+PlaylistItem::Role::Id, "ID");
    roles.insert(+PlaylistItem::Role::Artist, "Artist");
    roles.insert(+PlaylistItem::Role::Date, "Date");
    roles.insert(+PlaylistItem::Role::Duration, "Duration");
    roles.insert(+PlaylistItem::Role::Cover, "Cover");
    roles.insert(+PlaylistItem::Role::Number, "TrackNumber");
    roles.insert(+PlaylistItem::Role::PlayCount, "PlayCount");
    roles.insert(+PlaylistItem::Role::MultiDisk, "Multiple Discs");
    roles.insert(+PlaylistItem::Role::Playing, "IsPlaying");
    roles.insert(+PlaylistItem::Role::Path, "Path");
    roles.insert(+PlaylistItem::Role::Data, "Data");

    return roles;
}

QModelIndex PlaylistModel::matchTrack(int id) const
{
    QModelIndexList stack{};
    while(!stack.isEmpty()) {
        const QModelIndex parent = stack.takeFirst();
        for(int i = 0; i < rowCount(parent); ++i) {
            const QModelIndex child = index(i, 0, parent);
            if(rowCount(child) > 0) {
                stack.append(child);
            }
            else {
                const auto* item        = static_cast<PlaylistItem*>(child.internalPointer());
                const Core::Track track = std::get<Core::Track>(item->data());
                if(track.id() == id) {
                    return child;
                }
            }
        }
    }
    return {};
}

void PlaylistModel::reset()
{
    m_resetting = true;
    beginResetModel();
    beginReset();
    setupModelData();
    endResetModel();
    m_resetting = false;
}

void PlaylistModel::setupModelData()
{
    if(!m_playlistHandler->activePlaylist()) {
        return;
    }
    const Core::TrackList tracks = m_playlistHandler->activePlaylist()->tracks();

    if(tracks.empty()) {
        return;
    }

    // Create albums before model to ensure discs (based on discCount) are properly created
    createAlbums(tracks);

    for(const Core::Track& track : tracks) {
        if(!m_nodes.count(track.hash())) {
            if(auto* parent = iterateTrack(track, m_discHeaders, m_splitDiscs)) {
                checkInsertKey(track.hash(), PlaylistItem::Track, track, parent);
            }
        }
    }
}

void PlaylistModel::changeTrackState()
{
    emit dataChanged({}, {}, {PlaylistItem::Role::Playing});
}

QModelIndex PlaylistModel::indexForTrack(const Core::Track& track) const
{
    QModelIndex index;
    const auto key = track.hash();
    if(m_nodes.count(key)) {
        const auto* item = m_nodes.at(key).get();
        index            = createIndex(item->row(), 0, item);
    }
    return index;
}

QModelIndex PlaylistModel::indexForItem(PlaylistItem* item) const
{
    QModelIndex index;
    const auto key = item->key();
    if(m_nodes.count(key)) {
        const auto* item = m_nodes.at(key).get();
        index            = createIndex(item->row(), 0, item);
    }
    return index;
}

void PlaylistModel::createAlbums(const Core::TrackList& tracks)
{
    for(const auto& track : tracks) {
        if(!m_nodes.count(track.hash())) {
            const QString albumKey = track.albumHash();
            if(!m_albums.count(albumKey)) {
                Core::Album album{track.album()};
                album.setDate(track.date());
                album.setArtist(track.albumArtist());
                album.setCoverPath(track.coverPath());
                m_albums.emplace(albumKey, album);
            }
            Core::Album& album = m_albums.at(albumKey);
            album.addTrack(track);
        }
    }
}

PlaylistItem* PlaylistModel::iterateTrack(const Core::Track& track, bool discHeaders, bool splitDiscs)
{
    PlaylistItem* parent = nullptr;

    const QString albumKey = track.albumHash();

    if(m_albums.count(albumKey)) {
        auto& album           = m_albums.at(albumKey);
        const QString discKey = albumKey + QString::number(track.discNumber());
        const bool singleDisk = album.isSingleDiscAlbum() || (!splitDiscs && !discHeaders);

        if(singleDisk) {
            parent = checkInsertKey(albumKey, PlaylistItem::Album, &album, rootItem());
        }

        else if(splitDiscs) {
            if(!m_albums.count(discKey)) {
                Core::Album discAlbum{album};
                const QString discTitle = "Disc #" + QString::number(track.discNumber());
                discAlbum.setSubTitle(discTitle);
                discAlbum.reset();
                auto& addedAlbum = m_albums.emplace(discKey, std::move(discAlbum)).first->second;
                checkInsertKey(discKey, PlaylistItem::Album, &addedAlbum, rootItem());
            }
            Core::Album& discAlbum = m_albums.at(discKey);
            discAlbum.addTrack(track);
            parent = m_nodes.at(discKey).get();
        }

        else {
            if(!m_containers.count(discKey)) {
                Core::Container disc{"Disc #" + QString::number(track.discNumber())};
                PlaylistItem* parentNode = checkInsertKey(albumKey, PlaylistItem::Album, &album, rootItem());
                auto& addedDisc          = m_containers.emplace(discKey, std::move(disc)).first->second;
                checkInsertKey(discKey, PlaylistItem::Container, &addedDisc, parentNode);
            }
            Core::Container& disc = m_containers.at(discKey);
            disc.addTrack(track);
            parent = m_nodes.at(discKey).get();
        }
    }
    return parent;
}

PlaylistItem* PlaylistModel::checkInsertKey(const QString& key, PlaylistItem::Type type, const ItemType& item,
                                            PlaylistItem* parent)
{
    if(!m_nodes.count(key)) {
        auto* node = m_nodes.emplace(key, std::make_unique<PlaylistItem>(type, item, parent)).first->second.get();
        node->setKey(key);
    }
    PlaylistItem* child = m_nodes.at(key).get();
    if(Utils::contains(parent->children(), child)) {
        return child;
    }
    if(m_resetting) {
        parent->appendChild(child);
    }
    else {
        insertRow(parent, child);
    }
    return child;
}

void PlaylistModel::insertRow(PlaylistItem* parent, PlaylistItem* child)
{
    const int row = parent->childCount();
    beginInsertRows(indexForItem(parent), row, row);
    parent->appendChild(child);
    endInsertRows();
}

void PlaylistModel::beginReset()
{
    m_containers.clear();
    m_albums.clear();
    m_nodes.clear();
    resetRoot();
}

QVariant PlaylistModel::trackData(PlaylistItem* item, int role) const
{
    const Core::Track track = std::get<Core::Track>(item->data());

    switch(role) {
        case(PlaylistItem::Role::Id): {
            return track.id();
        }
        case(PlaylistItem::Role::Number): {
            return QStringLiteral("%1").arg(track.trackNumber(), 2, 10, QLatin1Char('0'));
        }
        case(Qt::DisplayRole): {
            return !track.title().isEmpty() ? track.title() : "Unknown Title";
        }
        case(PlaylistItem::Role::Artist): {
            return trackArtistString(track);
        }
        case(PlaylistItem::Role::PlayCount): {
            const int count = track.playCount();
            if(count > 0) {
                return QString::number(count) + QString("|");
            }
            return {};
        }
        case(PlaylistItem::Role::Duration): {
            return Utils::msToString(track.duration());
        }
        case(PlaylistItem::Role::MultiDisk): {
            return item->parent()->type() != PlaylistItem::Type::Album && m_discHeaders && !m_splitDiscs;
        }
        case(PlaylistItem::Role::Playing): {
            return m_playerManager->currentTrack() == track;
        }
        case(PlaylistItem::Role::Path): {
            return track.filepath();
        }
        case(PlaylistItem::Role::Data): {
            return QVariant::fromValue<Core::Track>(track);
        }
        case(Qt::BackgroundRole): {
            return m_altColours && !(item->row() & 1) ? QPalette::AlternateBase : QPalette::Base;
        }
        case(Qt::DecorationRole): {
            switch(m_playerManager->playState()) {
                case(Core::Player::PlayState::Playing):
                    return m_playingIcon;
                case(Core::Player::PlayState::Paused):
                    return m_pausedIcon;
                default:
                    break;
            }
            break;
        }
        default: {
            return {};
        }
    }
    return {};
}

QVariant PlaylistModel::albumData(PlaylistItem* item, int role) const
{
    const auto* album = std::get<Core::Album*>(item->data());

    if(!album) {
        return {};
    }

    switch(role) {
        case(Qt::DisplayRole): {
            QString title = !album->title().isEmpty() ? album->title() : "Unknown Title";
            if(!album->subTitle().isEmpty()) {
                title += " \u25AA ";
                title += album->subTitle();
            }
            return title;
        }
        case(PlaylistItem::Role::Cover): {
            return m_coverProvider.albumThumbnail(*album);
        }
        case(PlaylistItem::Role::Artist): {
            return !album->artist().isEmpty() ? album->artist() : "Unknown Artist";
        }
        case(PlaylistItem::Role::Duration): {
            const auto genre    = album->genres().join(" / ");
            const auto count    = album->trackCount();
            const auto duration = album->duration();

            QString dur = genre;
            if(!genre.isEmpty()) {
                dur += " | ";
            }
            dur += QString(QString::number(count) + (count > 1 ? " Tracks" : " Track") + " | "
                           + Utils::msToString(duration));
            return dur;
        }
        case(PlaylistItem::Role::Date): {
            return album->date();
        }
    }
    return {};
}

QVariant PlaylistModel::containerData(PlaylistItem* item, int role) const
{
    const auto* container = std::get<Core::Container*>(item->data());

    switch(role) {
        case(Qt::DisplayRole): {
            return container->title();
        }
        case(PlaylistItem::Role::Duration): {
            auto duration = static_cast<int>(container->duration());
            return QString(Utils::msToString(duration));
        }
        default: {
            return {};
        }
    }
}
} // namespace Fy::Gui::Widgets
