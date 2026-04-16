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

#include "moc_countdown-timer-dock.cpp"
#include "countdown-timer-dock.hpp"

#include <obs-module.h>

#include <QMainWindow>
#include <QByteArray>
#include <QLatin1StringView>


void CountdownTimerDock::Create()
{
    QMainWindow *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    static CountdownTimerDock *dock = new CountdownTimerDock(main);

    obs_frontend_add_dock_by_id("countdown-timer-dock", obs_module_text("CountdownTimerDock.Title"), dock);

    obs_frontend_add_event_callback(CountdownTimerDock::SceneChangeEvent, dock);
    obs_frontend_add_save_callback(CountdownTimerDock::SaveCountdownTimer, dock);
}

CountdownTimerDock::CountdownTimerDock(QWidget *parent)
    : QWidget(parent)
{
    CreateLayout();
}

void CountdownTimerDock::CreateLayout()
{
    /*
    * Possibly vertical layout, top is grid layout, middle is buttons, bottom is horizontal layout with start/stop buttons
    */
    layout = new QVBoxLayout();

    QGridLayout *gridLayout = new QGridLayout();

    switchScene = new QPushButton(tr("Switch to Countdown Scene"));
    startStopCountdown = new QPushButton(tr("Start Countdown"));

    timeEdit = new QTimeEdit();
    targetTextSource = new QComboBox();
    sourceScene = new QComboBox();
    targetScene = new QComboBox();

    timeEdit->setDisplayFormat("hh:mm:ss ap");

    startStopCountdown->setCheckable(true);
    startStopCountdown->setChecked(false);

    gridLayout->addWidget(new QLabel(tr("Transition to Target at Time:")), 0, 0);
    gridLayout->addWidget(new QLabel(tr("Timer Text Source:")), 1, 0);
    gridLayout->addWidget(new QLabel(tr("Transition Timer Scene:")), 2, 0);
    gridLayout->addWidget(new QLabel(tr("Transition Target Scene:")), 3, 0);

    gridLayout->addWidget(timeEdit, 0, 1);
    gridLayout->addWidget(targetTextSource, 1, 1);
    gridLayout->addWidget(sourceScene, 2, 1);
    gridLayout->addWidget(targetScene, 3, 1);

    layout->addLayout(gridLayout);
    layout->addWidget(switchScene);
    layout->addWidget(startStopCountdown);

    layout->addStretch();

    setLayout(layout);

    tickTimer = new QTimer(this);

    // Connect timer timeout to a handler
    connect(tickTimer, &QTimer::timeout, this, &CountdownTimerDock::OnTick);

    // Start button: start the timer with 100 ms interval
    connect(startStopCountdown, &QPushButton::clicked, this, [this]() {
        if (IsValidStartTime() == false)
        {
            this->startStopCountdown->setChecked(false);
            obs_log(LOG_INFO, "Countdown timer start is in the past, skipping...");
            return;
        }

        if (!tickTimer->isActive()) {
            tickTimer->start(100);
            obs_log(LOG_INFO, "Countdown timer started (100 ms tick)");
            this->startStopCountdown->setText(tr("Stop Countdown"));
            this->startStopCountdown->setChecked(true);
        } else {
            tickTimer->stop();
            obs_log(LOG_INFO, "Countdown timer stopped");
            this->startStopCountdown->setText(tr("Start Countdown"));
            this->startStopCountdown->setChecked(false);
        }
    });

    connect(switchScene, &QPushButton::clicked, this, [this]() {
        // Get the selected target text source name
        const QString scene = sourceScene->currentText();
        if (scene.isEmpty())
            return;

        const QByteArray sceneBa = scene.toUtf8();
        obs_source_t *sceneSrc = obs_get_source_by_name(sceneBa.constData());
        if (sceneSrc) {
            obs_frontend_set_current_scene(sceneSrc);
            obs_log(LOG_INFO, "Switched to countdown scene '%s' as configured", sceneBa.constData());
            obs_source_release(sceneSrc);
        } else {
            obs_log(LOG_WARNING, "CountdownTimerDock::OnTick: could not find countdown scene '%s'", sceneBa.constData());
        }
    });

    validationTimer = new QTimer(this);
    validationTimer->start(1000);

    connect(validationTimer, &QTimer::timeout, this, &CountdownTimerDock::OnValidationTick);
    connect(timeEdit, &QTimeEdit::userTimeChanged, this, &CountdownTimerDock::OnTimeChanged);
}

bool CountdownTimerDock::IsValidStartTime()
{
    int msRemaining = QTime::currentTime().msecsTo(timeEdit->time());

    return msRemaining > 0;
}

void CountdownTimerDock::OnValidationTick()
{
    startStopCountdown->setEnabled(IsValidStartTime());
}

void CountdownTimerDock::OnTimeChanged(QTime)
{
    startStopCountdown->setEnabled(IsValidStartTime());
}

void CountdownTimerDock::OnTick()
{
    // Get the selected target text source name
    const QString targetName = targetTextSource->currentText();
    if (targetName.isEmpty())
        return;

    // Resolve the OBS source by name
    const QByteArray nameBa = targetName.toUtf8();
    obs_source_t *src = obs_get_source_by_name(nameBa.constData());
    if (!src) {
        obs_log(LOG_WARNING, "CountdownTimerDock::OnTick: could not find source '%s'", nameBa.constData());
        return;
    }

    // Compute milliseconds remaining from now until the QTimeEdit target time.
    int msRemaining = QTime::currentTime().msecsTo(timeEdit->time());
    if (msRemaining < 0) {
        msRemaining = 0;

        tickTimer->stop();

        startStopCountdown->setText(tr("Start Countdown"));
        startStopCountdown->setChecked(false);
        startStopCountdown->setEnabled(false);

        const QString targetSceneName = targetScene->currentText();
        if (!targetSceneName.isEmpty()) {
            const QByteArray sceneBa = targetSceneName.toUtf8();
            obs_source_t *sceneSrc = obs_get_source_by_name(sceneBa.constData());
            if (sceneSrc) {
                obs_frontend_set_current_scene(sceneSrc);
                obs_log(LOG_INFO, "Switched to target scene '%s' as configured", sceneBa.constData());
                obs_source_release(sceneSrc);
            } else {
                obs_log(LOG_WARNING, "CountdownTimerDock::OnTick: could not find target scene '%s'",
                        sceneBa.constData());
            }
        } else {
            obs_log(LOG_INFO, "CountdownTimerDock::OnTick: no target scene configured, not switching");
        }
    }

    // Format remaining time. Show "H:MM:SS.t" if hours > 0, otherwise "MM:SS.t"
    int totalSeconds = msRemaining / 1000;
    #if 0
    int tenths = (msRemaining % 1000) / 100;
    #endif
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;

    QString text;
    #if 0
    if (hours > 0) {
        text = QString("%1:%2:%3.%4")
                   .arg(hours)
                   .arg(minutes, 2, 10, QChar('0'))
                   .arg(seconds, 2, 10, QChar('0'))
                   .arg(tenths);
    } else {
        text = QString("%1:%2.%3")
                   .arg(minutes, 2, 10, QChar('0'))
                   .arg(seconds, 2, 10, QChar('0'))
                   .arg(tenths);
    }
    #else
    if (hours > 0) {
        text = QString("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
    } else {
        text = QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
    }
    #endif

    // Update OBS text source setting named "text" with the formatted remaining time.
    const QByteArray textBa = text.toUtf8();
    obs_data_t *settings = obs_source_get_settings(src);
    if (!settings)
        settings = obs_data_create();

    obs_data_set_string(settings, "text", textBa.constData());
    obs_source_update(src, settings);

    // Release resources
    obs_data_release(settings);
    obs_source_release(src);
}

void CountdownTimerDock::ClearSceneList()
{
    sourceScene->clear();
    targetScene->clear();
}

void CountdownTimerDock::ClearSourceList()
{
    targetTextSource->clear();
}

void CountdownTimerDock::AddScene(const char *name)
{
    sourceScene->addItem(name);
    targetScene->addItem(name);
}

void CountdownTimerDock::AddSource(const char *name)
{
    targetTextSource->addItem(name);
}

void CountdownTimerDock::SceneChangeEvent(enum obs_frontend_event event, void *data)
{
    if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED || event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
        obs_log(LOG_INFO, "Scene changed/finished loading received in CountdownTimerDock::SceneChangeEvent");

        CountdownTimerDock *dock = static_cast<CountdownTimerDock *>(data);

        dock->ClearSceneList();
        dock->ClearSourceList();

        // Re-enumerate sources so the dock can update its scene list.
        obs_enum_sources(&CountdownTimerDock::UpdateSource, dock);
        obs_enum_scenes(&CountdownTimerDock::UpdateScene, dock);

        dock->targetTextSource->setCurrentIndex(dock->targetTextSource->findText(dock->targetTextSourceName));
        dock->sourceScene->setCurrentIndex(dock->sourceScene->findText(dock->sourceSceneName));
        dock->targetScene->setCurrentIndex(dock->targetScene->findText(dock->targetSceneName));
    }
}

void CountdownTimerDock::SaveCountdownTimer(obs_data_t* save_data, bool saving, void *data)
{
    CountdownTimerDock *dock = static_cast<CountdownTimerDock *>(data);

    if (saving)
    {
        obs_log(LOG_INFO, "Saving countdown timer configuration");

        OBSDataAutoRelease obj = obs_data_create();

        obs_data_set_string(obj, "targetTextSource", dock->targetTextSource->currentText().toStdString().c_str());
        obs_data_set_string(obj, "sourceScene", dock->sourceScene->currentText().toStdString().c_str());
        obs_data_set_string(obj, "targetScene", dock->targetScene->currentText().toStdString().c_str());
        obs_data_set_string(obj, "targetTransitionTime",
                            dock->timeEdit->time().toString("hh:mm:ss ap").toStdString().c_str());

        obs_data_set_obj(save_data, "countdown-timer-dock", obj);
    }
    else
    {
        obs_log(LOG_INFO, "Loading countdown timer configuration");

        OBSDataAutoRelease obj = obs_data_get_obj(save_data, "countdown-timer-dock");

        if (!obj) {
            obs_log(LOG_INFO, "No saved configuration found for countdown timer dock, using defaults");

            obj = obs_data_create();
            return;
        }

        dock->targetTextSourceName = obs_data_get_string(obj, "targetTextSource");
        dock->sourceSceneName = obs_data_get_string(obj, "sourceScene");
        dock->targetSceneName = obs_data_get_string(obj, "targetScene");
        dock->targetTransitionTime = obs_data_get_string(obj, "targetTransitionTime");

        obs_log(LOG_INFO, "Loaded countdown timer configuration: targetTextSource=%s, sourceScene=%s, targetScene=%s, targetTransitionTime=%s",
                dock->targetTextSourceName ? dock->targetTextSourceName : "null",
                dock->sourceSceneName ? dock->sourceSceneName : "null",
                dock->targetSceneName ? dock->targetSceneName : "null",
                dock->targetTransitionTime ? dock->targetTransitionTime : "null");

        dock->targetTextSource->setCurrentIndex(dock->targetTextSource->findText(dock->targetTextSourceName));
        dock->sourceScene->setCurrentIndex(dock->sourceScene->findText(dock->sourceSceneName));
        dock->targetScene->setCurrentIndex(dock->targetScene->findText(dock->targetSceneName));

        QTime transitionTime = QTime::fromString(dock->targetTransitionTime, "hh:mm:ss ap");

        if (transitionTime.isValid())
        {
            dock->timeEdit->setTime(transitionTime);
        }
    }
}

bool CountdownTimerDock::UpdateSource(void *param, obs_source_t *source)
{
    CountdownTimerDock *dock = static_cast<CountdownTimerDock *>(param);
    const char *name = obs_source_get_name(source);
    const char *id = obs_source_get_id(source);

    if (QLatin1StringView(id).contains(QLatin1StringView("text"))) {
        dock->AddSource(name);
        obs_log(LOG_INFO, "Added source to targetTextSource combo box: %s", name);
    } else {
        obs_log(LOG_INFO, "Skipping source: %s (%s)", name, id);
    }
    return true;
}

bool CountdownTimerDock::UpdateScene(void *param, obs_source_t *source)
{
    CountdownTimerDock *dock = static_cast<CountdownTimerDock *>(param);
    const char *name = obs_source_get_name(source);
    dock->AddScene(name);
    obs_log(LOG_INFO, "Added scene to sourceScene and targetScene combo boxes: %s", name);
    return true;
}
