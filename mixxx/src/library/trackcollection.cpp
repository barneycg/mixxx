#include <QtCore>
#include <QtGui>
#include <QtSql>
#include <QtDebug>

#include "trackcollection.h"
#include "xmlparse.h"
#include "defs.h"
#include "soundsourceproxy.h"
#include "library/schemamanager.h"
#include "trackinfoobject.h"
#include "library/librarytablemodel.h"

TrackCollection::TrackCollection(ConfigObject<ConfigValue>* pConfig)
        : m_pConfig(pConfig),
          m_db(QSqlDatabase::addDatabase("QSQLITE")),
          m_playlistDao(m_db),
          m_cueDao(m_db),
          m_trackDao(m_db, m_cueDao, pConfig),
          m_crateDao(m_db),
          m_supportedFileExtensionsRegex(SoundSourceProxy::supportedFileExtensionsRegex(),
                                         Qt::CaseInsensitive)
{

    bCancelLibraryScan = 0;
    qDebug() << QSqlDatabase::drivers();

    m_db.setHostName("localhost");
    m_db.setDatabaseName(MIXXX_DB_PATH);
    m_db.setUserName("mixxx");
    m_db.setPassword("mixxx");
    bool ok = m_db.open();
    qDebug() << __FILE__ << "DB status:" << ok;
    qDebug() << m_db.lastError();

    //Check for tables and create them if missing
    if (!checkForTables()) exit(-1);

    QStringList columns;
    columns << "library." + LIBRARYTABLE_ID
            << "library." + LIBRARYTABLE_PLAYED
            << "library." + LIBRARYTABLE_TIMESPLAYED
            << "library." + LIBRARYTABLE_ARTIST
            << "library." + LIBRARYTABLE_TITLE
            << "library." + LIBRARYTABLE_ALBUM
            << "library." + LIBRARYTABLE_YEAR
            << "library." + LIBRARYTABLE_DURATION
            << "library." + LIBRARYTABLE_RATING
            << "library." + LIBRARYTABLE_GENRE
            << "library." + LIBRARYTABLE_FILETYPE
            << "library." + LIBRARYTABLE_TRACKNUMBER
            << "library." + LIBRARYTABLE_KEY
            << "library." + LIBRARYTABLE_DATETIMEADDED
            << "library." + LIBRARYTABLE_BPM
            << "library." + LIBRARYTABLE_BITRATE
            << "track_locations.location"
            << "track_locations.fs_deleted"
            << "library." + LIBRARYTABLE_COMMENT
            << "library." + LIBRARYTABLE_MIXXXDELETED;

    QSqlQuery query(m_db);
    QString queryString = "CREATE TEMPORARY VIEW IF NOT EXISTS library_cache_view AS SELECT "
            + columns.join(",") +
            " FROM library INNER JOIN track_locations "
            "ON library.location = track_locations.id "
            "WHERE (" + LibraryTableModel::DEFAULT_LIBRARYFILTER + ")";

    for (QStringList::iterator it = columns.begin();
         it != columns.end(); ++it) {
        if (it->startsWith("library.")) {
            *it = it->replace("library.", "");
        } else if (it->startsWith("track_locations.")) {
            *it = it->replace("track_locations.", "");
        }
    }

    query.prepare(queryString);
    if (!query.exec()) {
        qDebug() << query.executedQuery() << query.lastError();
    }

    // Print out any SQL error, if there was one.
    if (query.lastError().isValid()) {
        qDebug() << __FILE__ << __LINE__ << query.lastError();
    }

    // Strip out library. and track_locations.
    for (int i = 0; i < columns.size(); ++i) {
        columns[i] = columns[i].replace("library.", "").replace("track_locations.", "");
    }

    BaseTrackCache* pBaseTrackCache = new BaseTrackCache(
        this, "library_cache_view", LIBRARYTABLE_ID, columns, true);
    connect(&m_trackDao, SIGNAL(trackDirty(int)),
            pBaseTrackCache, SLOT(slotTrackDirty(int)));
    connect(&m_trackDao, SIGNAL(trackChanged(int)),
            pBaseTrackCache, SLOT(slotTrackChanged(int)));
    connect(&m_trackDao, SIGNAL(trackClean(int)),
            pBaseTrackCache, SLOT(slotTrackClean(int)));
    connect(&m_trackDao, SIGNAL(tracksAdded(QSet<int>)),
            pBaseTrackCache, SLOT(slotTracksAdded(QSet<int>)));
    connect(&m_trackDao, SIGNAL(tracksRemoved(QSet<int>)),
            pBaseTrackCache, SLOT(slotTracksRemoved(QSet<int>)));

    addTrackSource(QString("default"),
                   QSharedPointer<BaseTrackCache>(pBaseTrackCache));
}

TrackCollection::~TrackCollection()
{
    // Save all tracks that haven't been saved yet.
    m_trackDao.saveDirtyTracks();
    // TODO(XXX) Maybe fold saveDirtyTracks into TrackDAO::finish now that it
    // exists? -- rryan 10/2010
    m_trackDao.finish();

    Q_ASSERT(!m_db.rollback()); //Rollback any uncommitted transaction
    //The above is an ASSERT because there should never be an outstanding
    //transaction when this code is called. If there is, it means we probably
    //aren't committing a transaction somewhere that should be.
    m_db.close();
    qDebug() << "TrackCollection destroyed";
}

bool TrackCollection::checkForTables()
{
    if (!m_db.open()) {
        QMessageBox::critical(0, tr("Cannot open database"),
                              tr("Unable to establish a database connection.\n"
                                 "Mixxx requires QT with SQLite support. Please read "
                                 "the Qt SQL driver documentation for information on how "
                                 "to build it.\n\n"
                                 "Click OK to exit."), QMessageBox::Ok);
        return false;
    }

    int requiredSchemaVersion = 8;
    if (!SchemaManager::upgradeToSchemaVersion(m_pConfig, m_db,
                                               requiredSchemaVersion)) {
        QMessageBox::warning(0, tr("Cannot upgrade database schema"),
                             tr("Unable to upgrade your database schema to version %1.\n"
                                "Your mixxx.db file may be corrupt.\n"
                                "Try renaming it and restarting Mixxx.\n\n"
                                "Click OK to exit.").arg(requiredSchemaVersion),
                             QMessageBox::Ok);
        return false;
    }

    m_trackDao.initialize();
    m_playlistDao.initialize();
    m_crateDao.initialize();
    m_cueDao.initialize();

    return true;
}


QSqlDatabase& TrackCollection::getDatabase()
{
 	return m_db;
  }


/** Do a non-recursive import of all the songs in a directory. Does NOT decend into subdirectories.
    @param trackDao The track data access object which provides a connection to the database. We use this parameter in order to make this function callable from separate threads. You need to use a different DB connection for each thread.
    @return true if the scan completed without being cancelled. False if the scan was cancelled part-way through.
*/
bool TrackCollection::importDirectory(QString directory, TrackDAO &trackDao,
                                      QList<TrackInfoObject*>& tracksToAdd)
{
    //qDebug() << "TrackCollection::importDirectory(" << directory<< ")";

    emit(startedLoading());
    QFileInfoList files;

    //Check to make sure the path exists.
    QDir dir(directory);
    if (dir.exists()) {
        files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    } else {
        qDebug() << "Error: Import path does not exist." << directory;
        return true;
    }

    //The directory exists, so get a list of the contents of the directory and go through it.
    QList<QFileInfo>::iterator it = files.begin();
    while (it != files.end()) {
        QFileInfo file = *it; //TODO: THIS IS SLOW!
        it++;

        //If a flag was raised telling us to cancel the library scan then stop.
        m_libraryScanMutex.lock();
        bool cancel = bCancelLibraryScan;
        m_libraryScanMutex.unlock();
        if (cancel)
        {
            return false;
        }

        QString absoluteFilePath = file.absoluteFilePath();
        QString fileName = file.fileName();

        if (fileName.count(m_supportedFileExtensionsRegex)) {
            //If the track is in the database, mark it as existing. This code gets exectuted
            //when other files in the same directory have changed (the directory hash has changed).
            trackDao.markTrackLocationAsVerified(absoluteFilePath);

            // If the file already exists in the database, continue and go on to
            // the next file.

            // If the file doesn't already exist in the database, then add
            // it. If it does exist in the database, then it is either in the
            // user's library OR the user has "removed" the track via
            // "Right-Click -> Remove". These tracks stay in the library, but
            // their mixxx_deleted column is 1.
            if (!trackDao.trackExistsInDatabase(absoluteFilePath))
            {
                //qDebug() << "Loading" << file.fileName();
                emit(progressLoading(fileName));

                // addTrack uses this QFileInfo instead of making a new one now.
                //trackDao.addTrack(file);
                TrackInfoObject* pTrack = new TrackInfoObject(file.absoluteFilePath());
                tracksToAdd.append(pTrack);
            }
        } else {
            //qDebug() << "Skipping" << file.fileName() <<
            //    "because it did not match thesupported audio files filter:" <<
        }
    }
    emit(finishedLoading());
    return true;
}



void TrackCollection::slotCancelLibraryScan()
{
    m_libraryScanMutex.lock();
    bCancelLibraryScan = 1;
    m_libraryScanMutex.unlock();
}

void TrackCollection::resetLibaryCancellation()
{
    m_libraryScanMutex.lock();
    bCancelLibraryScan = 0;
    m_libraryScanMutex.unlock();
}

CrateDAO& TrackCollection::getCrateDAO() {
    return m_crateDao;
}

TrackDAO& TrackCollection::getTrackDAO() {
    return m_trackDao;
}

PlaylistDAO& TrackCollection::getPlaylistDAO() {
    return m_playlistDao;
}

QSharedPointer<BaseTrackCache> TrackCollection::getTrackSource(const QString name) {
    return m_trackSources.value(name, QSharedPointer<BaseTrackCache>());
}

void TrackCollection::addTrackSource(const QString name,
                                     QSharedPointer<BaseTrackCache> trackSource) {
    Q_ASSERT(!m_trackSources.contains(name));
    m_trackSources[name] = trackSource;
}
