#ifndef WTRACKPROPERTY_H
#define WTRACKPROPERTY_H

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMouseEvent>

#include "preferences/usersettings.h"
#include "skin/skincontext.h"
#include "trackinfoobject.h"
#include "widget/wlabel.h"

class WTrackProperty : public WLabel {
    Q_OBJECT
  public:
    WTrackProperty(const char* group, UserSettingsPointer pConfig, QWidget* pParent);
    ~WTrackProperty() override;

    void setup(QDomNode node, const SkinContext& context) override;

  signals:
    void trackDropped(QString filename, QString group);

  public slots:
    void slotTrackLoaded(TrackPointer track);
    void slotLoadingTrack(TrackPointer pNewTrack, TrackPointer pOldTrack);

  private slots:
    void updateLabel(TrackInfoObject*);

  private:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

    const char* m_pGroup;
    UserSettingsPointer m_pConfig;
    TrackPointer m_pCurrentTrack;
    QString m_property;
};


#endif /* WTRACKPROPERTY_H */
