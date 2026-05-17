/*
Countdown Timer Dock
Copyright (C) 2026 Jessica Hamilton

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#pragma once

#include <obs.hpp>
#include <obs-frontend-api.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimeEdit>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QTimer>
#include <QString>

#include <plugin-support.h>

using namespace Qt::StringLiterals;

typedef bool (*SourcesCallback)(void *param, obs_source_t *source);

class CountdownTimerDock : public QWidget {
    Q_OBJECT

public:
    static void Create();

    CountdownTimerDock(QWidget *parent = nullptr);

    void SaveSettings();

private:
    bool IsValidStartTime();
    void UpdateScenesAndSources();

private:
    static void SceneChangeEvent(enum obs_frontend_event event, void *data);

    static void SaveCountdownTimer(obs_data_t *save_data, bool saving, void *);

private:
    void CreateLayout();

private slots:
    void OnTick();
    void OnValidationTick();
    void OnTimeChanged(QTime time);

private:
    QVBoxLayout *layout = nullptr;
    QWidget *mainWidget = nullptr;
    QTimeEdit *timeEdit = nullptr;
    // Don't think I need a QPlainTextEdit, could just be a second label, perhaps?
    QComboBox *targetTextSource = nullptr;
    QComboBox *sourceScene = nullptr;
    QComboBox *targetScene = nullptr;

    QPushButton *switchScene = nullptr;
    QPushButton *startStopCountdown = nullptr;

    QTimer *tickTimer = nullptr;
    QTimer *validationTimer = nullptr;

    QString targetTextSourceName = u""_s;
    QString sourceSceneName = u""_s;
    QString targetSceneName = u""_s;
    QString targetTransitionTime = u"00:00:00 am"_s;
};
