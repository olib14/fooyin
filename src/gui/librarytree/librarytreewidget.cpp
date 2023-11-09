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

#include "librarytreewidget.h"

#include "librarytreeappearance.h"
#include "librarytreegroupregistry.h"
#include "librarytreemodel.h"
#include "librarytreeview.h"

#include <core/library/musiclibrary.h>
#include <core/library/tracksort.h>
#include <gui/guisettings.h>
#include <gui/trackselectioncontroller.h>
#include <utils/async.h>

#include <QContextMenuEvent>
#include <QHeaderView>
#include <QJsonObject>
#include <QMenu>
#include <QTreeView>
#include <QVBoxLayout>

#include <QCoro/QCoroCore>

using namespace Qt::Literals::StringLiterals;

namespace Fy::Gui::Widgets {
void getLowestIndexes(const QTreeView* treeView, const QModelIndex& index, QModelIndexList& bottomIndexes)
{
    if(!index.isValid()) {
        getLowestIndexes(treeView, treeView->rootIndex(), bottomIndexes);
        return;
    }

    const int rowCount = treeView->model()->rowCount(index);
    if(rowCount == 0) {
        bottomIndexes.append(index);
        return;
    }

    for(int row = 0; row < rowCount; ++row) {
        const QModelIndex childIndex = treeView->model()->index(row, 0, index);
        getLowestIndexes(treeView, childIndex, bottomIndexes);
    }
}

class LibraryTreeWidgetPrivate
{
public:
    LibraryTreeWidgetPrivate(LibraryTreeWidget* self, Core::Library::MusicLibrary* library,
                             LibraryTreeGroupRegistry* groupsRegistry, TrackSelectionController* trackSelection,
                             Utils::SettingsManager* settings);

    void reset() const;

    void changeGrouping(const LibraryTreeGrouping& newGrouping);
    void addGroupMenu(QMenu* parent);

    void setScrollbarEnabled(bool enabled) const;
    void updateAppearance(const QVariant& optionsVar) const;

    void setupHeaderContextMenu(const QPoint& pos);

    QCoro::Task<void> selectionChanged() const;
    [[nodiscard]] QString playlistNameFromSelection() const;

    void handleDoubleClick() const;
    void handleMiddleClick() const;

    LibraryTreeWidget* self;

    Core::Library::MusicLibrary* library;
    LibraryTreeGroupRegistry* groupsRegistry;
    TrackSelectionController* trackSelection;
    Utils::SettingsManager* settings;

    LibraryTreeGrouping grouping;

    QVBoxLayout* layout;
    LibraryTreeView* libraryTree;
    LibraryTreeModel* model;

    TrackAction doubleClickAction;
    TrackAction middleClickAction;
};

LibraryTreeWidgetPrivate::LibraryTreeWidgetPrivate(LibraryTreeWidget* self, Core::Library::MusicLibrary* library,
                                                   LibraryTreeGroupRegistry* groupsRegistry,
                                                   TrackSelectionController* trackSelection,
                                                   Utils::SettingsManager* settings)
    : self{self}
    , library{library}
    , groupsRegistry{groupsRegistry}
    , trackSelection{trackSelection}
    , settings{settings}
    , layout{new QVBoxLayout(self)}
    , libraryTree{new LibraryTreeView(self)}
    , model{new LibraryTreeModel(self)}
    , doubleClickAction{static_cast<TrackAction>(settings->value<Settings::LibraryTreeDoubleClick>())}
    , middleClickAction{static_cast<TrackAction>(settings->value<Settings::LibraryTreeMiddleClick>())}
{
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(libraryTree);
    libraryTree->setModel(model);

    libraryTree->setExpandsOnDoubleClick(doubleClickAction == TrackAction::Expand);
    libraryTree->setHeaderHidden(!settings->value<Settings::LibraryTreeHeader>());
    setScrollbarEnabled(settings->value<Settings::LibraryTreeScrollBar>());
    libraryTree->setAlternatingRowColors(settings->value<Settings::LibraryTreeAltColours>());

    changeGrouping(groupsRegistry->itemByName(u""_s));

    if(!library->isEmpty()) {
        reset();
    }

    updateAppearance(settings->value<Settings::LibraryTreeAppearance>());
}

void LibraryTreeWidgetPrivate::reset() const
{
    model->reset(library->tracks());
}

void LibraryTreeWidgetPrivate::changeGrouping(const LibraryTreeGrouping& newGrouping)
{
    grouping = newGrouping;
    model->changeGrouping(grouping);
    reset();
}

void LibraryTreeWidgetPrivate::addGroupMenu(QMenu* parent)
{
    auto* groupMenu = new QMenu(u"Grouping"_s, parent);

    const auto& groups = groupsRegistry->items();
    for(const auto& group : groups) {
        auto* switchGroup = new QAction(group.second.name, groupMenu);
        QObject::connect(switchGroup, &QAction::triggered, self, [this, group]() { changeGrouping(group.second); });
        groupMenu->addAction(switchGroup);
    }
    parent->addMenu(groupMenu);
}

void LibraryTreeWidgetPrivate::setScrollbarEnabled(bool enabled) const
{
    libraryTree->setVerticalScrollBarPolicy(enabled ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
}

void LibraryTreeWidgetPrivate::updateAppearance(const QVariant& optionsVar) const
{
    const auto options = optionsVar.value<LibraryTreeAppearance>();
    model->setAppearance(options);
    QMetaObject::invokeMethod(libraryTree->itemDelegate(), "sizeHintChanged", Q_ARG(QModelIndex, {}));
}

void LibraryTreeWidgetPrivate::setupHeaderContextMenu(const QPoint& pos)
{
    auto* menu = new QMenu(self);
    addGroupMenu(menu);
    menu->popup(self->mapToGlobal(pos));
}

QCoro::Task<void> LibraryTreeWidgetPrivate::selectionChanged() const
{
    const QModelIndexList selectedIndexes = libraryTree->selectionModel()->selectedIndexes();
    if(selectedIndexes.empty()) {
        co_return;
    }

    QModelIndexList trackIndexes;
    for(const QModelIndex& index : selectedIndexes) {
        getLowestIndexes(libraryTree, index, trackIndexes);
    }

    Core::TrackList tracks;
    for(const auto& index : trackIndexes) {
        const int level = index.data(LibraryTreeRole::Level).toInt();
        if(level < 0) {
            tracks.clear();
            tracks = library->tracks();
            break;
        }
        const auto indexTracks = index.data(LibraryTreeRole::Tracks).value<Core::TrackList>();
        tracks.insert(tracks.end(), indexTracks.cbegin(), indexTracks.cend());
    }

    const auto sortedTracks
        = co_await Utils::asyncExec([&tracks]() { return Core::Library::Sorting::sortTracks(tracks); });
    trackSelection->changeSelectedTracks(sortedTracks, playlistNameFromSelection());

    if(settings->value<Settings::LibraryTreePlaylistEnabled>()) {
        const QString playlistName = settings->value<Settings::LibraryTreeAutoPlaylist>();
        const bool autoSwitch      = settings->value<Settings::LibraryTreeAutoSwitch>();

        trackSelection->executeAction(TrackAction::SendNewPlaylist, autoSwitch ? Switch : None, playlistName);
    }
    co_return;
}

QString LibraryTreeWidgetPrivate::playlistNameFromSelection() const
{
    QString title;
    const QModelIndexList selectedIndexes = libraryTree->selectionModel()->selectedIndexes();
    for(const auto& index : selectedIndexes) {
        if(!title.isEmpty()) {
            title.append(", ");
        }
        title.append(index.data().toString());
    }
    return title;
}

void LibraryTreeWidgetPrivate::handleDoubleClick() const
{
    const bool autoSwitch = settings->value<Settings::LibraryTreeAutoSwitch>();
    trackSelection->executeAction(doubleClickAction, autoSwitch ? Switch : None, playlistNameFromSelection());
}

void LibraryTreeWidgetPrivate::handleMiddleClick() const
{
    const bool autoSwitch = settings->value<Settings::LibraryTreeAutoSwitch>();
    trackSelection->executeAction(middleClickAction, autoSwitch ? Switch : None, playlistNameFromSelection());
}

LibraryTreeWidget::LibraryTreeWidget(Core::Library::MusicLibrary* library, LibraryTreeGroupRegistry* groupsRegistry,
                                     TrackSelectionController* trackSelection, Utils::SettingsManager* settings,
                                     QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<LibraryTreeWidgetPrivate>(this, library, groupsRegistry, trackSelection, settings)}
{
    setObjectName(LibraryTreeWidget::name());

    QObject::connect(p->libraryTree, &LibraryTreeView::doubleClicked, this, [this]() { p->handleDoubleClick(); });
    QObject::connect(p->libraryTree, &LibraryTreeView::middleMouseClicked, this, [this]() { p->handleMiddleClick(); });
    QObject::connect(p->libraryTree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this]() { p->selectionChanged(); });
    QObject::connect(p->libraryTree->header(), &QHeaderView::customContextMenuRequested, this,
                     [this](const QPoint& pos) { p->setupHeaderContextMenu(pos); });
    QObject::connect(groupsRegistry, &LibraryTreeGroupRegistry::groupingChanged, this,
                     [this](const LibraryTreeGrouping& changedGrouping) {
                         if(p->grouping.id == changedGrouping.id) {
                             p->changeGrouping(changedGrouping);
                         }
                     });

    auto treeReset = [this]() {
        p->reset();
    };

    QObject::connect(library, &Core::Library::MusicLibrary::tracksLoaded, this, treeReset);
    QObject::connect(library, &Core::Library::MusicLibrary::tracksAdded, p->model, &LibraryTreeModel::addTracks);
    QObject::connect(library, &Core::Library::MusicLibrary::tracksUpdated, p->model, &LibraryTreeModel::updateTracks);
    QObject::connect(library, &Core::Library::MusicLibrary::tracksDeleted, p->model, &LibraryTreeModel::removeTracks);
    QObject::connect(library, &Core::Library::MusicLibrary::tracksSorted, this, treeReset);
    QObject::connect(library, &Core::Library::MusicLibrary::libraryRemoved, this, treeReset);
    QObject::connect(library, &Core::Library::MusicLibrary::libraryChanged, this, treeReset);

    settings->subscribe<Settings::LibraryTreeDoubleClick>(this, [this](int action) {
        p->doubleClickAction = static_cast<TrackAction>(action);
        p->libraryTree->setExpandsOnDoubleClick(p->doubleClickAction == TrackAction::Expand);
    });
    settings->subscribe<Settings::LibraryTreeMiddleClick>(
        this, [this](int action) { p->middleClickAction = static_cast<TrackAction>(action); });
    settings->subscribe<Settings::LibraryTreeHeader>(this,
                                                     [this](bool show) { p->libraryTree->setHeaderHidden(!show); });
    settings->subscribe<Settings::LibraryTreeScrollBar>(this, [this](bool show) { p->setScrollbarEnabled(show); });
    settings->subscribe<Settings::LibraryTreeAltColours>(
        this, [this](bool enable) { p->libraryTree->setAlternatingRowColors(enable); });
    settings->subscribe<Settings::LibraryTreeAppearance>(this,
                                                         [this](const QVariant& var) { p->updateAppearance(var); });
}

QString LibraryTreeWidget::name() const
{
    return u"Library Tree"_s;
}

QString LibraryTreeWidget::layoutName() const
{
    return u"LibraryTree"_s;
}

void LibraryTreeWidget::saveLayout(QJsonArray& array)
{
    QJsonObject options;
    options["Grouping"_L1] = p->grouping.name;

    QJsonObject tree;
    tree[layoutName()] = options;
    array.append(tree);
}

void LibraryTreeWidget::loadLayout(const QJsonObject& object)
{
    const LibraryTreeGrouping grouping = p->groupsRegistry->itemByName(object["Grouping"_L1].toString());
    if(grouping.isValid()) {
        p->changeGrouping(grouping);
    }
}

void LibraryTreeWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    p->trackSelection->addTrackPlaylistContextMenu(menu);
    p->addGroupMenu(menu);
    p->trackSelection->addTrackContextMenu(menu);

    menu->popup(mapToGlobal(event->pos()));
}
} // namespace Fy::Gui::Widgets
