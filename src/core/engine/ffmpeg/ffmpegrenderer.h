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

#include "ffmpegframe.h"
#include "ffmpegworker.h"

namespace Fy::Core::Engine {
class AudioOutput;

namespace FFmpeg {
class Codec;

class Renderer : public EngineWorker
{
    Q_OBJECT

public:
    Renderer(QObject* parent = nullptr);
    ~Renderer() override;

    void run(Codec* codec, AudioOutput* output);
    void reset() override;
    void kill() override;

    void pauseOutput(bool isPaused);
    void updateOutput(AudioOutput* output);
    void updateDevice(const QString& device);
    void updateVolume(double volume);

public slots:
    void render(Frame frame);

signals:
    void frameProcessed(Frame frame);

private:
    bool canDoNextStep() const override;
    int timerInterval() const override;
    void doNextStep() override;

    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace FFmpeg
} // namespace Fy::Core::Engine