/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "settingspages.h"

#include "core/library/libraryinfo.h"
#include "core/library/librarymanager.h"
#include "utils/settings.h"

#include <QCheckBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>

GeneralPage::GeneralPage(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    //    mainLayout->addStretch();
    mainLayout->setAlignment(Qt::AlignTop);
    setLayout(mainLayout);
}

GeneralPage::~GeneralPage() = default;

LibraryPage::LibraryPage(Library::LibraryManager* libraryManager, QWidget* parent)
    : QWidget(parent)
    , m_libraryManager(libraryManager)
    , m_libraryList(0, 3, this)
{
    auto libraries = m_libraryManager->allLibraries();

    m_libraryList.setHorizontalHeaderLabels({"ID", "Name", "Path"});
    m_libraryList.verticalHeader()->hide();
    m_libraryList.horizontalHeader()->setStretchLastSection(true);
    m_libraryList.setSelectionBehavior(QAbstractItemView::SelectRows);

    for(const auto& lib : libraries)
    {
        addLibraryRow(lib);
    }
    m_libraryList.hideColumn(0);

    auto* libraryButtons = new QWidget(this);
    auto* libraryButtonLayout = new QVBoxLayout(this);
    auto* addLibrary = new QPushButton("+", this);
    auto* removeLibrary = new QPushButton("-", this);

    libraryButtonLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    libraryButtons->setLayout(libraryButtonLayout);
    libraryButtonLayout->addWidget(addLibrary);
    libraryButtonLayout->addWidget(removeLibrary);

    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->addWidget(&m_libraryList);
    mainLayout->addWidget(libraryButtons);
    // mainLayout->addStretch();
    setLayout(mainLayout);

    connect(addLibrary, &QPushButton::clicked, this, [=] {
        bool ok = false;
        QString name = "";
        QString text = QInputDialog::getText(this, tr("QInputDialog::getText()"), tr("Library Name:"),
                                             QLineEdit::Normal, QDir::home().dirName(), &ok);
        if(ok && !text.isEmpty())
        {
            name = text;
        }

        QString newDir
            = QFileDialog::getExistingDirectory(this, "Directory", QDir::homePath(), QFileDialog::ShowDirsOnly);

        if(newDir.isEmpty())
        {
            return;
        }

        const auto id = m_libraryManager->addLibrary(newDir, name);
        const auto lib = m_libraryManager->libraryInfo(id);
        addLibraryRow(lib);
    });
    connect(removeLibrary, &QPushButton::clicked, this, [=] {
        const auto selItems = m_libraryList.selectionModel()->selectedRows();
        for(const auto& item : selItems)
        {
            const int row = item.row();
            const int id = m_libraryList.item(row, 0)->text().toInt();
            m_libraryManager->removeLibrary(id);
            m_libraryList.removeRow(row);
        }
    });
}

LibraryPage::~LibraryPage() = default;

void LibraryPage::addLibraryRow(const Library::LibraryInfo& info)
{
    const int row = m_libraryList.rowCount();
    m_libraryList.setRowCount(row + 1);

    auto* libId = new QTableWidgetItem{QString::number(info.id())};
    auto* libName = new QTableWidgetItem{info.name()};
    auto* libPath = new QTableWidgetItem{info.path()};

    m_libraryList.setItem(row, 0, libId);
    m_libraryList.setItem(row, 1, libName);
    m_libraryList.setItem(row, 2, libPath);
}

PlaylistPage::PlaylistPage(QWidget* parent)
    : QWidget(parent)
{
    auto* settings = Settings::instance();

    auto* groupHeaders = new QCheckBox("Enable Disc Headers", this);
    groupHeaders->setChecked(settings->value(Settings::Setting::DiscHeaders).toBool());

    auto* splitDiscs = new QCheckBox("Split Discs", this);
    splitDiscs->setChecked(settings->value(Settings::Setting::SplitDiscs).toBool());
    splitDiscs->setEnabled(groupHeaders->isChecked());

    auto* simpleList = new QCheckBox("Simple Playlist", this);
    simpleList->setChecked(settings->value(Settings::Setting::SimplePlaylist).toBool());

    auto* altColours = new QCheckBox("Alternate Row Colours", this);
    altColours->setChecked(settings->value(Settings::Setting::PlaylistAltColours).toBool());

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(groupHeaders);
    mainLayout->addWidget(splitDiscs);
    mainLayout->addWidget(simpleList);
    mainLayout->addWidget(altColours);
    mainLayout->addStretch();
    setLayout(mainLayout);

    connect(groupHeaders, &QCheckBox::clicked, this, [=](bool checked) {
        settings->set(Settings::Setting::DiscHeaders, checked);
        if(checked)
        {
            splitDiscs->setEnabled(checked);
        }
        else
        {
            splitDiscs->setChecked(checked);
            splitDiscs->setEnabled(checked);
        }
    });
    connect(splitDiscs, &QCheckBox::clicked, this, [=](bool checked) {
        settings->set(Settings::Setting::SplitDiscs, checked);
    });
    connect(simpleList, &QCheckBox::clicked, this, [=](bool checked) {
        settings->set(Settings::Setting::SimplePlaylist, checked);
    });
    connect(altColours, &QCheckBox::clicked, this, [=](bool checked) {
        settings->set(Settings::Setting::PlaylistAltColours, checked);
    });
}

PlaylistPage::~PlaylistPage() = default;
