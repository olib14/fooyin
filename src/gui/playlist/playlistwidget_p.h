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

#include "playlistpreset.h"

#include <core/player/playermanager.h>

#include <QCoroTask>

#include <QString>

class QHBoxLayout;
class QMenu;

namespace Fy {

namespace Utils {
class ActionManager;
class SettingsManager;
class SettingsDialogController;
class HeaderView;
class WidgetContext;
} // namespace Utils

namespace Core {
namespace Player {
class PlayerManager;
}
namespace Playlist {
class Playlist;
}
} // namespace Core

namespace Gui {
class TrackSelectionController;

namespace Widgets::Playlist {
class PlaylistWidget;
class PlaylistController;
class PlaylistModel;
class PlaylistView;

class PlaylistWidgetPrivate : public QObject
{
    Q_OBJECT

public:
    PlaylistWidgetPrivate(PlaylistWidget* self, Utils::ActionManager* actionManager,
                          Core::Player::PlayerManager* playerManager, PlaylistController* playlistController,
                          TrackSelectionController* selectionController, Utils::SettingsManager* settings);

    void setupConnections();
    void setupActions();

    void onPresetChanged(const PlaylistPreset& preset);
    void changePreset(const PlaylistPreset& preset);

    void changePlaylist(Core::Playlist::Playlist* playlist) const;

    void resetTree() const;

    [[nodiscard]] bool isHeaderHidden() const;
    [[nodiscard]] bool isScrollbarHidden() const;

    void setHeaderHidden(bool showHeader) const;
    void setScrollbarHidden(bool showScrollBar) const;

    void selectionChanged();
    void playlistTracksChanged() const;
    void tracksRemoved() const;

    void customHeaderMenuRequested(QPoint pos);

    void changeState(Core::Player::PlayState state) const;

    void doubleClicked(const QModelIndex& index) const;

    void followCurrentTrack(const Core::Track& track, int index) const;

    void switchContextMenu(int section, QPoint pos);
    QCoro::Task<void> changeSort(QString script) const;
    void addSortMenu(QMenu* parent);


    PlaylistWidget* self;

    Utils::ActionManager* actionManager;
    Core::Player::PlayerManager* playerManager;
    TrackSelectionController* selectionController;
    Utils::SettingsManager* settings;
    Utils::SettingsDialogController* settingsDialog;

    PlaylistController* playlistController;

    QHBoxLayout* layout;
    PlaylistModel* model;
    PlaylistView* playlistView;
    Utils::HeaderView* header;
    bool changingSelection{false};

    PlaylistPreset currentPreset;

    Utils::WidgetContext* playlistContext;
};
} // namespace Widgets::Playlist
} // namespace Gui
} // namespace Fy