#include <QtDebug>
#include <QtCore>
#include <windows.h>
#include <Psapi.h>

#include "aboutdialog.h"
#include "addfeeddialog.h"
#include "optionsdialog.h"
#include "rsslisting.h"
#include "VersionNo.h"
#include "delegatewithoutfocus.h"

/*!****************************************************************************/
const QString kDbName = "feeds.db";

const QString kCreateFeedsTableQuery(
    "create table feeds("
        "id integer primary key, "
        "text varchar, "             // ����� ����� (������ �������� ���)
        "title varchar, "            // ��� �����
        "description varchar, "      // �������� �����
        "xmlurl varchar, "           // ��������-����� ����� �����
        "htmlurl varchar, "          // ��������-����� �����, � �������� �������� �����
        "language varchar, "         // ����, �� ������� �������� �����
        "copyrights varchar, "       // �����
        "author_name varchar, "      // ����� �����: ���
        "author_email varchar, "     //              �-����
        "author_uri varchar, "       //              ������ ��������
        "webMaster varchar, "        // �-���� ����� �������������� �� ����������� ��������� �����
        "pubdate varchar, "          // ���� ���������� ����������� �����
        "lastBuildDate varchar, "    // ��������� ���� ��������� ����������� �����
        "category varchar, "         // ��������� �����������, ���������� � �����
        "contributor varchar, "      // �������� (����� ����)
        "generator varchar, "        // ���������, ������������ ��� ��������� �����������
        "docs varchar, "             // ������ �� ��������, ����������� �������� RSS
        "cloud_domain varchar, "     // ���-������, ��������������� rssCloud ���������
        "cloud_port varchar, "       //   .
        "cloud_path varchar, "       //   .
        "cloud_procedure varchar, "  //   .
        "cloud_protocal varchar, "   //   .
        "ttl integer, "              // ����� � �������, � ������� �������� ����� ����� ���� ���������
        "skipHours varchar, "        // ��������� ������������, ����� �� ����� ��������� ����� (����������� ����)
        "skipDays varchar, "         // ��������� ������������, ����� �� ����� ��������� ����� (����������� ��� ������)
        "image blob, "               // gif, jpeg, png �������, ������� ����� ���� ������������ � �������
        "unread integer, "           // ���������� ������������� ��������
        "newCount integer, "         // ���������� ����� ��������
        "currentNews integer, "      // ������������ �������
        "label varchar"              // ������������ �������������
    ")");

const QString kCreateNewsTableQuery(
    "create table feed_%1("
        "id integer primary key, "
        "feed integer, "                       // ������������� ����� �� ������� feeds
        "guid varchar, "                       // ���������� �����
        "guidislink varchar default 'true', "  // ���� ����, ��� ���������� ����� �������� ������� �� �������
        "description varchar, "                // ������� ����������
        "content varchar, "                    // ������ ���������� (atom)
        "title varchar, "                      // ���������
        "published varchar, "                  // ���� ����������
        "modified varchar, "                   // ���� �����������
        "received varchar, "                   // ���� ����� ������� (������������ ��� �����)
        "author_name varchar, "                // ��� ������
        "author_uri varchar, "                 // ��������� ������ (atom)
        "author_email varchar, "               // ����� ������ (atom)
        "category varchar, "                   // ���������, ����� ��������� ��������� ��������� (�������� ����� ���� ���������)
        "label varchar, "                      // ����� (������������ �������������)
        "new integer default 1, "              // ���� "�����". ��������������� ��� �����, ��������� ��� �������� ���������
        "read integer default 0, "             // ���� "�����������". ��������������� ����� ������ �������
        "sticky integer default 0, "           // ���� "��������". ��������������� �������������
        "deleted integer default 0, "          // ���� "��������". ������� ���������� ��������, �� ��������� �� ���� �� ���������, 
                                               //   ����� ��� ���������� �������� ��� �� ���������� �����. 
                                               //   ���������� �������� ������� ����� ������������ ��� ����� ������ ����
        "attachment varchar, "                 // ������ �� ������������� ����� (������ ����� ���� ��������� ������)
        "comments varchar, "                   // ��������-������ �� ��������, ���������� �����������(������) � �������
        "enclosure_length, "                   // �����-������, ��������������� � ��������:
        "enclosure_type, "                     //   �����, ���,
        "enclosure_url, "                      //   �����.
        "source varchar, "                     // ��������, ���� ��� ��������������  (atom: <link via>)
        "link_href varchar, "                  // ��������-������ �� ������� (atom: <link self>)
        "link_enclosure varchar, "             // ��������-������ �� ������������ ������� ����� ����������,
                                               //   ������� ��������� �������� � ������� (atom)
        "link_related varchar, "               // ��������-������ �� ������������� ������ ��� �������  (atom)
        "link_alternate varchar, "             // ��������-������ �� �������������� ������������� �������
        "contributor varchar, "                // �������� (����� ����)
        "rights varchar "                      // �����
    ")");

/*!****************************************************************************/
RSSListing::RSSListing(QWidget *parent)
    : QMainWindow(parent)
{
    QString AppFileName = qApp->applicationFilePath();
    AppFileName.replace(".exe", ".ini");
    settings_ = new QSettings(AppFileName, QSettings::IniFormat);

    QString dbFileName(qApp->applicationDirPath() + "/" + kDbName);
    db_ = QSqlDatabase::addDatabase("QSQLITE");
    db_.setDatabaseName(dbFileName);
    if (QFile(dbFileName).exists()) {
      db_.open();
    } else {  // ������������� ����
      db_.open();
      db_.exec(kCreateFeedsTableQuery);
      db_.exec("create table info(id integer primary key, name varchar, value varchar)");
      db_.exec("insert into info(name, value) values ('version', '1.0')");
    }

    persistentUpdateThread_ = new UpdateThread(this);
    persistentUpdateThread_->setObjectName("persistentUpdateThread_");
    connect(persistentUpdateThread_, SIGNAL(readedXml(QByteArray, QUrl)),
        this, SLOT(receiveXml(QByteArray, QUrl)));
    connect(persistentUpdateThread_, SIGNAL(getUrlDone(int)),
        this, SLOT(getUrlDone(int)));

    persistentParseThread_ = new ParseThread(this, &db_);
    persistentParseThread_->setObjectName("persistentParseThread_");
    connect(this, SIGNAL(xmlReadyParse(QByteArray,QUrl)),
        persistentParseThread_, SLOT(parseXml(QByteArray,QUrl)),
        Qt::QueuedConnection);

    feedsModel_ = new FeedsModel(this);
    feedsModel_->setTable("feeds");
    feedsModel_->select();

    feedsView_ = new QTreeView();
    feedsView_->setObjectName("feedsTreeView_");
    feedsView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    feedsView_->setModel(feedsModel_);
    for (int i = 0; i < feedsModel_->columnCount(); ++i)
      feedsView_->hideColumn(i);
    feedsView_->showColumn(feedsModel_->fieldIndex("text"));
    feedsView_->showColumn(feedsModel_->fieldIndex("unread"));
    feedsView_->header()->setStretchLastSection(false);
    feedsView_->header()->setResizeMode(feedsModel_->fieldIndex("text"), QHeaderView::Stretch);
    feedsView_->header()->setResizeMode(feedsModel_->fieldIndex("unread"), QHeaderView::ResizeToContents);

    feedsView_->header()->setVisible(false);
    feedsView_->setUniformRowHeights(true);
    feedsView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    feedsView_->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(feedsView_, SIGNAL(clicked(QModelIndex)),
            this, SLOT(slotFeedsTreeClicked(QModelIndex)));
    connect(this, SIGNAL(signalFeedsTreeKeyUpDownPressed()),
            SLOT(slotFeedsTreeKeyUpDownPressed()), Qt::QueuedConnection);
    connect(feedsView_, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(showContextMenuFeed(const QPoint &)));

    newsModel_ = new NewsModel(this);
    newsModel_->setEditStrategy(QSqlTableModel::OnFieldChange);
    newsView_ = new NewsView(this);
    newsView_->setModel(newsModel_);
    newsView_->model_ = newsModel_;
    newsModel_->view_ = newsView_;

    newsHeader_ = new NewsHeader(Qt::Horizontal, newsView_);
    newsHeader_->model_ = newsModel_;
    newsHeader_->view_ = newsView_;
    newsView_->setHeader(newsHeader_);

    connect(newsView_, SIGNAL(clicked(QModelIndex)),
            this, SLOT(slotNewsViewClicked(QModelIndex)));
    connect(this, SIGNAL(signalFeedKeyUpDownPressed()),
            SLOT(slotNewsKeyUpDownPressed()), Qt::QueuedConnection);
    connect(newsView_, SIGNAL(signalSetItemRead(QModelIndex, int)),
            this, SLOT(slotSetItemRead(QModelIndex, int)));
    connect(newsView_, SIGNAL(signalSetItemStar(QModelIndex,int)),
            this, SLOT(slotSetItemStar(QModelIndex,int)));
    connect(newsView_, SIGNAL(signalDoubleClicked(QModelIndex)),
            this, SLOT(slotNewsViewDoubleClicked(QModelIndex)));
    connect(newsView_, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(showContextMenuNews(const QPoint &)));

    webView_ = new QWebView();
    webView_->setObjectName("webView_");
    webView_->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
    webViewProgress_ = new QProgressBar(this);
    webViewProgress_->setObjectName("webViewProgress_");
    webViewProgress_->setFixedHeight(15);
    webViewProgress_->setMinimum(0);
    webViewProgress_->setMaximum(100);
    webViewProgress_->setFormat(tr("Loading... (%p%)"));
    webViewProgress_->setVisible(true);
    connect(webView_, SIGNAL(loadStarted()), this, SLOT(slotLoadStarted()));
    connect(webView_, SIGNAL(loadFinished(bool)), this, SLOT(slotLoadFinished(bool)));
    connect(webView_, SIGNAL(loadProgress(int)), webViewProgress_, SLOT(setValue(int)));
    connect(webView_, SIGNAL(linkClicked(QUrl)), this, SLOT(slotLinkClicked(QUrl)));

    setContextMenuPolicy(Qt::CustomContextMenu);

    QLabel *feedsTitleLabel_ = new QLabel(tr("Feeds"), this);
    feedsTitleLabel_->setObjectName("feedsTitleLabel_");
    feedsTitleLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    feedsTitleLabel_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);

    feedsToolBar_ = new QToolBar(this);
    feedsToolBar_->setObjectName("feedsToolBar_");
    feedsToolBar_->setIconSize(QSize(16, 16));

    QHBoxLayout *feedsPanelLayout = new QHBoxLayout();
    feedsPanelLayout->setMargin(0);
    feedsPanelLayout->setSpacing(0);

    feedsPanelLayout->addWidget(feedsTitleLabel_, 0);
    feedsPanelLayout->addStretch(1);
    feedsPanelLayout->addWidget(feedsToolBar_, 0);
    feedsPanelLayout->addSpacing(5);

    QWidget *feedsPanel = new QWidget(this);
    feedsPanel->setObjectName("feedsPanel");
    feedsPanel->setLayout(feedsPanelLayout);

    QVBoxLayout *feedsWidgetLayout = new QVBoxLayout();
    feedsWidgetLayout->setMargin(1);
    feedsWidgetLayout->setSpacing(0);
    feedsWidgetLayout->addWidget(feedsView_);

    QWidget *feedsWidget = new QWidget(this);
    feedsWidget->setObjectName("feedsWidget");
    feedsWidget->setLayout(feedsWidgetLayout);

    //! Create feeds DockWidget
    setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
    setCorner( Qt::TopRightCorner, Qt::RightDockWidgetArea );
    setDockOptions(QMainWindow::AnimatedDocks|QMainWindow::AllowNestedDocks);

    feedsDock_ = new QDockWidget(tr("Feeds"), this);
    feedsDock_->setObjectName("feedsDock");
    feedsDock_->setAllowedAreas(Qt::LeftDockWidgetArea|Qt::RightDockWidgetArea|Qt::TopDockWidgetArea);
    feedsDock_->setFeatures(QDockWidget::DockWidgetMovable);
    feedsDock_->setTitleBarWidget(feedsPanel);
    feedsDock_->setWidget(feedsWidget);
    addDockWidget(Qt::LeftDockWidgetArea, feedsDock_);
    connect(feedsDock_, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)),
        this, SLOT(slotFeedsDockLocationChanged(Qt::DockWidgetArea)));

    toolBarNull_ = new QToolBar(this);
    toolBarNull_->setObjectName("toolBarNull");
    toolBarNull_->setMovable(false);
    toolBarNull_->setFixedWidth(6);
    addToolBar(Qt::LeftToolBarArea, toolBarNull_);

    pushButtonNull_ = new QPushButton(this);
    pushButtonNull_->setObjectName("pushButtonNull");
    pushButtonNull_->setIcon(QIcon(":/images/images/triangleL.png"));
    pushButtonNull_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    toolBarNull_->addWidget(pushButtonNull_);
    connect(pushButtonNull_, SIGNAL(clicked()), this, SLOT(slotVisibledFeedsDock()));


    newsTitleLabel_ = new QLabel(tr("news"), this);
    newsTitleLabel_->setObjectName("newsTitleLabel_");
    newsTitleLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    newsTitleLabel_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);

    newsToolBar_ = new QToolBar(this);
    newsToolBar_->setObjectName("newsToolBar_");
    newsToolBar_->setIconSize(QSize(16, 16));

    QHBoxLayout *newsPanelLayout = new QHBoxLayout();
    newsPanelLayout->setMargin(0);
    newsPanelLayout->setSpacing(0);

    newsPanelLayout->addWidget(newsTitleLabel_, 0);
    newsPanelLayout->addStretch(1);
    newsPanelLayout->addWidget(newsToolBar_, 0);
    newsPanelLayout->addSpacing(5);

    QWidget *newsPanel = new QWidget(this);
    newsPanel->setObjectName("newsPanel");
    newsPanel->setLayout(newsPanelLayout);

    QVBoxLayout *newsWidgetLayout = new QVBoxLayout();
    newsWidgetLayout->setMargin(1);
    newsWidgetLayout->setSpacing(0);
    newsWidgetLayout->addWidget(newsView_);

    QWidget *newsWidget = new QWidget(this);
    newsWidget->setObjectName("newsWidget");
    newsWidget->setLayout(newsWidgetLayout);

    //! Create news DockWidget
    newsDock_ = new QDockWidget(tr("News"), this);
    newsDock_->setObjectName("newsDock");
    newsDock_->setFeatures(QDockWidget::DockWidgetMovable);
    newsDock_->setTitleBarWidget(newsPanel);
    newsDock_->setWidget(newsWidget);
    addDockWidget(Qt::TopDockWidgetArea, newsDock_);
    connect(newsDock_, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)),
        this, SLOT(slotNewsDockLocationChanged(Qt::DockWidgetArea)));

    webPanelAuthorLabel_ = new QLabel(tr("Author:"));

    QVBoxLayout *webPanelLabelLayout = new QVBoxLayout();
    webPanelLabelLayout->addWidget(new QLabel(tr("Title:")));
    webPanelLabelLayout->addWidget(webPanelAuthorLabel_);

    webPanelAuthor_ = new QLabel("");
    webPanelAuthor_->setObjectName("webPanelAuthor_");
    webPanelAuthor_->setOpenExternalLinks(true);

    webPanelTitle_ = new QLabel("");
    webPanelTitle_->setObjectName("webPanelTitle_");
    webPanelTitle_->setOpenExternalLinks(true);

    QVBoxLayout *webPanelTitleLayout = new QVBoxLayout();
    webPanelTitleLayout->addWidget(webPanelTitle_);
    webPanelTitleLayout->addWidget(webPanelAuthor_);

    QHBoxLayout *webPanelLayout = new QHBoxLayout();
    webPanelLayout->addLayout(webPanelLabelLayout, 0);
    webPanelLayout->addLayout(webPanelTitleLayout, 1);

    webPanel_ = new QWidget();
    webPanel_->setObjectName("webPanel_");
    webPanel_->setLayout(webPanelLayout);

    //! Create web layout
    QVBoxLayout *webLayout = new QVBoxLayout();
    webLayout->setMargin(1);  // ����� ���� ����� ������� �������
    webLayout->setSpacing(0);
    webLayout->addWidget(webPanel_, 0);
    webLayout->addWidget(webView_, 1);
    webLayout->addWidget(webViewProgress_, 0);

    webWidget_ = new QWidget();
    webWidget_->setObjectName("webWidget_");
    webWidget_->setLayout(webLayout);
    webWidget_->setMinimumWidth(400);

    setCentralWidget(webWidget_);

    setWindowTitle(QString("QuiteRSS v") + QString(STRFILEVER).section('.', 0, 2));

    createActions();
    createMenu();
    createToolBar();
    createMenuNews();
    createMenuFeed();

    feedsView_->installEventFilter(this);
    newsView_->installEventFilter(this);
    toolBarNull_->installEventFilter(this);

    //! GIU tuning
    progressBar_ = new QProgressBar();
    progressBar_->setObjectName("progressBar_");
    progressBar_->setFixedWidth(200);
    progressBar_->setFixedHeight(15);
    progressBar_->setMinimum(0);
    progressBar_->setFormat(tr("Update feeds... (%p%)"));
    progressBar_->setVisible(false);
    statusBar()->addPermanentWidget(progressBar_);
    statusUnread_ = new QLabel(tr(" Unread: "));
    statusBar()->addPermanentWidget(statusUnread_);
    statusAll_ = new QLabel(tr(" All: "));
    statusBar()->addPermanentWidget(statusAll_);
    statusBar()->setVisible(true);

    traySystem = new QSystemTrayIcon(QIcon(":/images/images/QtRSS16.png"),this);
    connect(traySystem,SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(slotActivationTray(QSystemTrayIcon::ActivationReason)));
    connect(this,SIGNAL(signalPlaceToTray()),this,SLOT(slotPlaceToTray()),Qt::QueuedConnection);
    traySystem->setToolTip("QtRSS");
    createTrayMenu();
    traySystem->show();

    connect(this, SIGNAL(signalCloseApp()),
            SLOT(slotCloseApp()), Qt::QueuedConnection);
    connect(feedsView_, SIGNAL(doubleClicked(QModelIndex)),
            updateFeedAct_, SLOT(trigger()));

    loadSettingsFeeds();
    int row = newsView_->currentIndex().row();

    readSettings();

    newsHeader_->createMenu();
    newsView_->setCurrentIndex(newsModel_->index(row, 0));

    //��������� ������� � �� �������� ��� ���������
//    QFont font_ = newsTitleLabel_->font();
//    font_.setBold(true);
//    newsTitleLabel_->setFont(font_);
}

/*!****************************************************************************/
RSSListing::~RSSListing()
{
  qDebug("App_Closing");

  db_.transaction();
  QSqlQuery q(db_);
  q.exec("select id from feeds");
  while (q.next()) {
    QSqlQuery qt(db_);
    QString qStr = QString("UPDATE feed_%1 SET new=0 WHERE new=1")
        .arg(q.value(0).toString());
    qt.exec(qStr);
  }
  db_.commit();

  QString  qStr = QString("update feeds set newCount=0");
  q.exec(qStr);

  persistentUpdateThread_->quit();
  persistentParseThread_->quit();

  delete newsView_;
  delete feedsView_;
  delete newsModel_;
  delete feedsModel_;

  db_.close();

  QSqlDatabase::removeDatabase(QString());
}

/*virtual*/ void RSSListing::showEvent(QShowEvent* event)
{
  connect(feedsDock_, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)),
          this, SLOT(slotDockLocationChanged(Qt::DockWidgetArea)), Qt::UniqueConnection);
}

/*!****************************************************************************/
bool RSSListing::eventFilter(QObject *obj, QEvent *event)
{
  if (obj == feedsView_) {
    if (event->type() == QEvent::KeyPress) {
      QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
      if ((keyEvent->key() == Qt::Key_Up) ||
          (keyEvent->key() == Qt::Key_Down)) {
        emit signalFeedsTreeKeyUpDownPressed();
      }
      return false;
    } else {
      return false;
    }
  } else if (obj == newsView_) {
    if (event->type() == QEvent::KeyPress) {
      QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
      if ((keyEvent->key() == Qt::Key_Up) ||
          (keyEvent->key() == Qt::Key_Down)) {
        emit signalFeedKeyUpDownPressed();
      } else if (keyEvent->key() == Qt::Key_Delete) {
        deleteNews();
      }
      return false;
    } else {
      return false;
    }
  } else if (obj == toolBarNull_) {
    if (event->type() == QEvent::MouseButtonRelease) {
      slotVisibledFeedsDock();
    }
    return false;
  } else {
    // pass the event on to the parent class
    return QMainWindow::eventFilter(obj, event);
  }
}

/*! \brief ��������� ������� �������� ���� ************************************/
/*virtual*/ void RSSListing::closeEvent(QCloseEvent* event)
{
  oldState = windowState();
  event->ignore();
  emit signalPlaceToTray();
}

/*! \brief ��������� ������� ������ �� ���������� *****************************/
void RSSListing::slotClose()
{
  traySystem->hide();
  hide();
  writeSettings();
  emit signalCloseApp();
}

/*! \brief ���������� ���������� **********************************************/
void RSSListing::slotCloseApp()
{
  qApp->quit();
}

/*! \brief ��������� ������� ��������� ��������� ���� *************************/
/*virtual*/ void RSSListing::changeEvent(QEvent *event)
{
  if(event->type() == QEvent::WindowStateChange) {
    oldState = ((QWindowStateChangeEvent*)event)->oldState();
    if(isMinimized()) {
      event->ignore();
      emit signalPlaceToTray();
      return;
    }
  }
  QMainWindow::changeEvent(event);
}

/*! \brief ��������� ������� ��������� ��������� � ���� ***********************/
void RSSListing::slotPlaceToTray()
{
  QTimer::singleShot(10000, this, SLOT(myEmptyWorkingSet()));
  hide();
}

/*! \brief ��������� ������� ���� *********************************************/
void RSSListing::slotActivationTray(QSystemTrayIcon::ActivationReason reason)
{
  switch (reason) {
  case QSystemTrayIcon::Unknown:
    break;
  case QSystemTrayIcon::Context:
    trayMenu_->activateWindow();
    break;
  case QSystemTrayIcon::DoubleClick:
    slotShowWindows();
    break;
  case QSystemTrayIcon::Trigger:
    break;
  case QSystemTrayIcon::MiddleClick:
    break;
  }
}

/*! \brief ����������� ���� �� ������� ****************************************/
void RSSListing::slotShowWindows()
{
  if (oldState == Qt::WindowMaximized) {
    showMaximized();
  } else {
    showNormal();
  }
  activateWindow();
}

/*! \brief �������� �������� **************************************************
 * \details ������� ����� �������������� � ������� ���� � ToolBar
 ******************************************************************************/
void RSSListing::createActions()
{
  addFeedAct_ = new QAction(QIcon(":/images/addFeed"), tr("&Add..."), this);
  addFeedAct_->setStatusTip(tr("Add new feed"));
  addFeedAct_->setShortcut(QKeySequence::New);
  connect(addFeedAct_, SIGNAL(triggered()), this, SLOT(addFeed()));

  deleteFeedAct_ = new QAction(QIcon(":/images/deleteFeed"), tr("&Delete..."), this);
  deleteFeedAct_->setStatusTip(tr("Delete selected feed"));
  connect(deleteFeedAct_, SIGNAL(triggered()), this, SLOT(deleteFeed()));

  importFeedsAct_ = new QAction(QIcon(":/images/importFeeds"), tr("&Import feeds..."), this);
  importFeedsAct_->setStatusTip(tr("Import feeds from OPML file"));
  connect(importFeedsAct_, SIGNAL(triggered()), this, SLOT(importFeeds()));

  exitAct_ = new QAction(tr("E&xit"), this);
  exitAct_->setShortcut(Qt::CTRL+Qt::Key_Q);  // standart on other OS
  connect(exitAct_, SIGNAL(triggered()), this, SLOT(slotClose()));

  toolBarToggle_ = new QAction(tr("&ToolBar"), this);
  toolBarToggle_->setCheckable(true);
  toolBarToggle_->setStatusTip(tr("Show ToolBar"));

  autoLoadImagesToggle_ = new QAction(QIcon(":/images/imagesOff"), tr("&Load images"), this);
  autoLoadImagesToggle_->setCheckable(true);
  autoLoadImagesToggle_->setStatusTip(tr("Auto load images to news view"));

  updateFeedAct_ = new QAction(QIcon(":/images/updateFeed"), tr("Update"), this);
  updateFeedAct_->setStatusTip(tr("Update current feed"));
  updateFeedAct_->setShortcut(Qt::Key_F5);
  connect(updateFeedAct_, SIGNAL(triggered()), this, SLOT(slotGetFeed()));

  updateAllFeedsAct_ = new QAction(QIcon(":/images/updateAllFeeds"), tr("Update all"), this);
  updateAllFeedsAct_->setStatusTip(tr("Update all feeds"));
  updateAllFeedsAct_->setShortcut(Qt::CTRL + Qt::Key_F5);
  connect(updateAllFeedsAct_, SIGNAL(triggered()), this, SLOT(slotGetAllFeeds()));

  markNewsRead_ = new QAction(QIcon(":/images/markRead"), tr("Mark Read"), this);
  markNewsRead_->setStatusTip(tr("Mark current news read"));
  connect(markNewsRead_, SIGNAL(triggered()), this, SLOT(markNewsRead()));

  markAllNewsRead_ = new QAction(QIcon(":/images/markReadAll"), tr("Mark all news Read"), this);
  markAllNewsRead_->setStatusTip(tr("Mark all news read"));
  connect(markAllNewsRead_, SIGNAL(triggered()), this, SLOT(markAllNewsRead()));

  optionsAct_ = new QAction(QIcon(":/images/options"), tr("Options..."), this);
  optionsAct_->setStatusTip(tr("Open options gialog"));
  optionsAct_->setShortcut(Qt::Key_F8);
  connect(optionsAct_, SIGNAL(triggered()), this, SLOT(showOptionDlg()));

  feedsFilter_ = new QAction(QIcon(":/images/filterOff"), tr("Filter"), this);
  filterFeedsAll_ = new QAction(tr("Show All"), this);
  filterFeedsAll_->setObjectName("filterFeedsAll_");
  filterFeedsAll_->setCheckable(true);
  filterFeedsAll_->setChecked(true);
  filterFeedsNew_ = new QAction(tr("Show New"), this);
  filterFeedsNew_->setObjectName("filterFeedsNew_");
  filterFeedsNew_->setCheckable(true);
  filterFeedsUnread_ = new QAction(tr("Show Unread"), this);
  filterFeedsUnread_->setObjectName("filterFeedsUnread_");
  filterFeedsUnread_->setCheckable(true);

  newsFilter_ = new QAction(QIcon(":/images/filterOff"), tr("Filter"), this);
  filterNewsAll_ = new QAction(tr("Show All"), this);
  filterNewsAll_->setObjectName("filterNewsAll_");
  filterNewsAll_->setCheckable(true);
  filterNewsAll_->setChecked(true);
  filterNewsNew_ = new QAction(tr("Show New"), this);
  filterNewsNew_->setObjectName("filterNewsNew_");
  filterNewsNew_->setCheckable(true);
  filterNewsUnread_ = new QAction(tr("Show Unread"), this);
  filterNewsUnread_->setObjectName("filterNewsUnread_");
  filterNewsUnread_->setCheckable(true);
  filterNewsStar_ = new QAction(tr("Show Star"), this);
  filterNewsStar_->setObjectName("filterNewsStar_");
  filterNewsStar_->setCheckable(true);

  aboutAct_ = new QAction(tr("About..."), this);
  aboutAct_->setObjectName("AboutAct_");
  aboutAct_->setToolTip(tr("Show 'About' dialog"));
  connect(aboutAct_, SIGNAL(triggered()), this, SLOT(slotShowAboutDlg()));

  openInBrowserAct_ = new QAction(tr("Open in Browser"), this);
  connect(openInBrowserAct_, SIGNAL(triggered()), this, SLOT(openInBrowserNews()));
  markStarAct_ = new QAction(QIcon(":/images/starOn"), tr("Star"), this);
  connect(markStarAct_, SIGNAL(triggered()), this, SLOT(markNewsStar()));
  deleteNewsAct_ = new QAction(QIcon(":/images/deleteNews"), tr("Delete"), this);
  deleteNewsAct_->setShortcut(Qt::Key_Delete);
  connect(deleteNewsAct_, SIGNAL(triggered()), this, SLOT(deleteNews()));

  markFeedRead_ = new QAction(QIcon(":/images/markRead"), tr("Mark Read"), this);
  markFeedRead_->setStatusTip(tr("Mark feed read"));
  connect(markFeedRead_, SIGNAL(triggered()), this, SLOT(markAllNewsRead()));
  feedProperties_ = new QAction(tr("Properties"), this);
  feedProperties_->setStatusTip(tr("Properties feed"));
}

/*! \brief �������� �������� ���� *********************************************/
void RSSListing::createMenu()
{
  fileMenu_ = menuBar()->addMenu(tr("&File"));
  fileMenu_->addAction(addFeedAct_);
  fileMenu_->addAction(deleteFeedAct_);
  fileMenu_->addSeparator();
  fileMenu_->addAction(importFeedsAct_);
  fileMenu_->addSeparator();
  fileMenu_->addAction(exitAct_);

  menuBar()->addMenu(tr("&Edit"));

  viewMenu_ = menuBar()->addMenu(tr("&View"));
  viewMenu_->addAction(toolBarToggle_);
  viewMenu_->addAction(autoLoadImagesToggle_);

  feedMenu_ = menuBar()->addMenu(tr("Fee&ds"));
  feedMenu_->addAction(updateFeedAct_);
  feedMenu_->addAction(updateAllFeedsAct_);
  feedMenu_->addSeparator();

  feedsFilterGroup_ = new QActionGroup(this);
  feedsFilterGroup_->setExclusive(true);
  connect(feedsFilterGroup_, SIGNAL(triggered(QAction*)), this, SLOT(setFeedsFilter(QAction*)));

  feedsFilterMenu_ = new QMenu(this);
  feedsFilterMenu_->addAction(filterFeedsAll_);
  feedsFilterGroup_->addAction(filterFeedsAll_);
  feedsFilterMenu_->addSeparator();
  feedsFilterMenu_->addAction(filterFeedsNew_);
  feedsFilterGroup_->addAction(filterFeedsNew_);
  feedsFilterMenu_->addAction(filterFeedsUnread_);
  feedsFilterGroup_->addAction(filterFeedsUnread_);

  feedsFilter_->setMenu(feedsFilterMenu_);
  feedMenu_->addAction(feedsFilter_);
  feedsToolBar_->addAction(feedsFilter_);
  connect(feedsFilter_, SIGNAL(triggered()), this, SLOT(slotFeedsFilter()));

  newsMenu_ = menuBar()->addMenu(tr("&News"));
  newsMenu_->addAction(markNewsRead_);
  newsMenu_->addAction(markAllNewsRead_);
  newsMenu_->addSeparator();

  newsFilterGroup_ = new QActionGroup(this);
  newsFilterGroup_->setExclusive(true);
  connect(newsFilterGroup_, SIGNAL(triggered(QAction*)), this, SLOT(setNewsFilter(QAction*)));

  newsFilterMenu_ = new QMenu(this);
  newsFilterMenu_->addAction(filterNewsAll_);
  newsFilterGroup_->addAction(filterNewsAll_);
  newsFilterMenu_->addSeparator();
  newsFilterMenu_->addAction(filterNewsNew_);
  newsFilterGroup_->addAction(filterNewsNew_);
  newsFilterMenu_->addAction(filterNewsUnread_);
  newsFilterGroup_->addAction(filterNewsUnread_);
  newsFilterMenu_->addAction(filterNewsStar_);
  newsFilterGroup_->addAction(filterNewsStar_);

  newsFilter_->setMenu(newsFilterMenu_);
  newsMenu_->addAction(newsFilter_);
  newsToolBar_->addAction(newsFilter_);
  connect(newsFilter_, SIGNAL(triggered()), this, SLOT(slotNewsFilter()));

  toolsMenu_ = menuBar()->addMenu(tr("&Tools"));
  toolsMenu_->addAction(optionsAct_);

  helpMenu_ = menuBar()->addMenu(tr("&Help"));
  helpMenu_->addAction(aboutAct_);
}

/*! \brief �������� ToolBar ***************************************************/
void RSSListing::createToolBar()
{
  toolBar_ = addToolBar(tr("ToolBar"));
  toolBar_->setObjectName("ToolBar_General");
  toolBar_->setAllowedAreas(Qt::TopToolBarArea);
  toolBar_->setMovable(false);
  toolBar_->addAction(addFeedAct_);
  toolBar_->addAction(deleteFeedAct_);
  toolBar_->addSeparator();
  toolBar_->addAction(updateFeedAct_);
  toolBar_->addAction(updateAllFeedsAct_);
  toolBar_->addSeparator();
  toolBar_->addAction(markNewsRead_);
  toolBar_->addAction(markAllNewsRead_);
  toolBar_->addSeparator();
  toolBar_->addAction(autoLoadImagesToggle_);
  toolBar_->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  connect(toolBar_, SIGNAL(visibilityChanged(bool)), toolBarToggle_, SLOT(setChecked(bool)));
  connect(toolBarToggle_, SIGNAL(toggled(bool)), toolBar_, SLOT(setVisible(bool)));
  connect(autoLoadImagesToggle_, SIGNAL(toggled(bool)), this, SLOT(setAutoLoadImages(bool)));
}

/*! \brief ������ �������� �� ini-����� ***************************************/
void RSSListing::readSettings()
{
  settings_->beginGroup("/Settings");

  QString fontFamily = settings_->value("/FontFamily", "Tahoma").toString();
  int fontSize = settings_->value("/FontSize", 8).toInt();
  qApp->setFont(QFont(fontFamily, fontSize));

  fontFamily = settings_->value("/WebFontFamily", "Tahoma").toString();
  fontSize = settings_->value("/WebFontSize", 12).toInt();
  webView_->settings()->setFontFamily(QWebSettings::StandardFont, fontFamily);
  webView_->settings()->setFontSize(QWebSettings::DefaultFontSize, fontSize);

  settings_->endGroup();

  restoreGeometry(settings_->value("GeometryState").toByteArray());
  restoreState(settings_->value("ToolBarsState").toByteArray());
  newsHeader_->restoreGeometry(settings_->value("NewsHeaderGeometry").toByteArray());
  newsHeader_->restoreState(settings_->value("NewsHeaderState").toByteArray());

  networkProxy_.setType(static_cast<QNetworkProxy::ProxyType>(
      settings_->value("networkProxy/type", QNetworkProxy::NoProxy).toInt()));
  networkProxy_.setHostName(settings_->value("networkProxy/hostName", "10.0.0.172").toString());
  networkProxy_.setPort(    settings_->value("networkProxy/port",     3150).toUInt());
  networkProxy_.setUser(    settings_->value("networkProxy/user",     "").toString());
  networkProxy_.setPassword(settings_->value("networkProxy/password", "").toString());
  persistentUpdateThread_->setProxy(networkProxy_);
}

/*! \brief ������ �������� � ini-���� *****************************************/
void RSSListing::writeSettings()
{
  settings_->beginGroup("/Settings");

  QString strLocalLang = QLocale::system().name();
  QString lang = settings_->value("/Lang", strLocalLang).toString();
  settings_->setValue("/Lang", lang);

  QString fontFamily = settings_->value("/FontFamily", "Tahoma").toString();
  settings_->setValue("/FontFamily", fontFamily);
  int fontSize = settings_->value("/FontSize", 8).toInt();
  settings_->setValue("/FontSize", fontSize);

  fontFamily = settings_->value("/WebFontFamily", "Tahoma").toString();
  settings_->setValue("/WebFontFamily", fontFamily);
  fontSize = settings_->value("/WebFontSize", 12).toInt();
  settings_->setValue("/WebFontSize", fontSize);

  settings_->setValue("autoLoadImages", autoLoadImagesToggle_->isChecked());

  settings_->endGroup();

  settings_->setValue("GeometryState", saveGeometry());
  settings_->setValue("ToolBarsState", saveState());
  if (newsModel_->columnCount()) {
    settings_->setValue("NewsHeaderGeometry", newsHeader_->saveGeometry());
    settings_->setValue("NewsHeaderState", newsHeader_->saveState());
  }

  settings_->setValue("networkProxy/type",     networkProxy_.type());
  settings_->setValue("networkProxy/hostName", networkProxy_.hostName());
  settings_->setValue("networkProxy/port",     networkProxy_.port());
  settings_->setValue("networkProxy/user",     networkProxy_.user());
  settings_->setValue("networkProxy/password", networkProxy_.password());

  settings_->setValue("feedSettings/currentId",
                      feedsModel_->index(feedsView_->currentIndex().row(), 0).data().toInt());
  settings_->setValue("feedSettings/filterName",
                      feedsFilterGroup_->checkedAction()->objectName());
  settings_->setValue("newsSettings/filterName",
                      newsFilterGroup_->checkedAction()->objectName());
}

/*! \brief ���������� ����� � ������ ���� *************************************/
void RSSListing::addFeed()
{
  AddFeedDialog *addFeedDialog = new AddFeedDialog(this);
  addFeedDialog->setWindowTitle(tr("Add feed"));
  if (addFeedDialog->exec() == QDialog::Rejected) return;

  QSqlQuery q(db_);
  QString qStr = QString("insert into feeds(text, xmlurl) values (?, ?)");
  q.prepare(qStr);
  q.addBindValue(addFeedDialog->feedTitleEdit_->text());
  q.addBindValue(addFeedDialog->feedUrlEdit_->text());
  q.exec();
  q.exec(kCreateNewsTableQuery.arg(q.lastInsertId().toString()));
  q.finish();

  QModelIndex index = feedsView_->currentIndex();
  feedsModel_->select();
  feedsView_->setCurrentIndex(index);
}

/*! \brief �������� ����� �� ������ ���� � �������������� *********************/
void RSSListing::deleteFeed()
{
  if (feedsView_->currentIndex().row() >= 0) {
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Question);
    msgBox.setWindowTitle(tr("Delete feed"));
    msgBox.setText(QString(tr("Are you sure to delete the feed '%1'?")).
                   arg(feedsModel_->record(feedsView_->currentIndex().row()).field("text").value().toString()));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    if (msgBox.exec() == QMessageBox::No) return;

    QSqlQuery q(db_);
    QString str = QString("delete from feeds where text='%1'").
        arg(feedsModel_->record(feedsView_->currentIndex().row()).field("text").value().toString());
    q.exec(str);
    q.exec(QString("drop table feed_%1").
        arg(feedsModel_->record(feedsView_->currentIndex().row()).field("id").value().toString()));
    q.finish();

    int row = feedsView_->currentIndex().row();
    feedsModel_->select();
    if (feedsModel_->rowCount() == row) row--;
    feedsView_->setCurrentIndex(feedsModel_->index(row, 0));
  }
}

/*! \brief ������ ���� �� OPML-����� ******************************************/
void RSSListing::importFeeds()
{
  QString fileName = QFileDialog::getOpenFileName(this, tr("Select OPML-file"),
      qApp->applicationDirPath(),
      tr("OPML-files (*.opml)"));

  if (fileName.isNull()) {
    statusBar()->showMessage(tr("Import canceled"), 3000);
    return;
  }

  qDebug() << "process file:" << fileName;

  QFile file(fileName);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    statusBar()->showMessage(tr("Import: can't open a file"), 3000);
    return;
  }

  db_.transaction();

  QXmlStreamReader xml(&file);

  int elementCount = 0;
  int outlineCount = 0;
  while (!xml.atEnd()) {
    xml.readNext();
    if (xml.isStartElement()) {
      statusBar()->showMessage(QVariant(elementCount).toString(), 3000);
      // �������� ���� outline'�
      if (xml.name() == "outline") {
        qDebug() << outlineCount << "+:" << xml.prefix().toString()
            << ":" << xml.name().toString();;
        QSqlQuery q(db_);
        QString qStr = QString("insert into feeds(text, title, description, xmlurl, htmlurl) "
                       "values(?, ?, ?, ?, ?)");
        q.prepare(qStr);
        q.addBindValue(xml.attributes().value("text").toString());
        q.addBindValue(xml.attributes().value("title").toString());
        q.addBindValue(xml.attributes().value("description").toString());
        q.addBindValue(xml.attributes().value("xmlUrl").toString());
        q.addBindValue(xml.attributes().value("htmlUrl").toString());
        q.exec();
        qDebug() << q.lastError().number() << ": " << q.lastError().text();
        q.exec(kCreateNewsTableQuery.arg(q.lastInsertId().toString()));
        q.finish();
      }
    } else if (xml.isEndElement()) {
      if (xml.name() == "outline") {
        ++outlineCount;
      }
      ++elementCount;
    }
  }
  if (xml.error()) {
    statusBar()->showMessage(QString("Import error: Line=%1, ErrorString=%2").
        arg(xml.lineNumber()).arg(xml.errorString()), 3000);
  } else {
    statusBar()->showMessage(QString("Import: file read done"), 3000);
  }
  db_.commit();

  QModelIndex index = feedsView_->currentIndex();
  feedsModel_->select();
  feedsView_->setCurrentIndex(index);
}

/*! \brief ���� xml-����� ****************************************************/
void RSSListing::receiveXml(const QByteArray &data, const QUrl &url)
{
  url_ = url;
  data_.append(data);
}

/*! \brief ��������� ��������� ������� ****************************************/
void RSSListing::getUrlDone(const int &result)
{
  qDebug() << "getUrl result =" << result;

  if (!url_.isEmpty()) {
    qDebug() << "emit xmlReadyParse: before <<" << url_;
    emit xmlReadyParse(data_, url_);
    qDebug() << "emit xmlReadyParse: after  <<" << url_;
    data_.clear();
    url_.clear();
  }

  // ������� �������� �����
  if (0 == result) {
    updateAllFeedsAct_->setEnabled(true);
    progressBar_->hide();
    statusBar()->showMessage(QString("Update done"), 3000);
  }
  // � ������� �������� �������� _result_ ��������
  else if (0 < result) {
    progressBar_->setValue(progressBar_->maximum() - result);
  }
}

void RSSListing::slotUpdateFeed(const QUrl &url)
{
  // ����� �������������� ����� � ������� ����
  int parseFeedId = 0;
  QSqlQuery q(db_);
  q.exec(QString("select id from feeds where xmlurl like '%1'").
      arg(url.toString()));
  while (q.next()) {
    parseFeedId = q.value(q.record().indexOf("id")).toInt();
  }

  int unreadCount = 0;
  QString qStr = QString("select count(read) from feed_%1 where read==0").
      arg(parseFeedId);
  q.exec(qStr);
  if (q.next()) unreadCount = q.value(0).toInt();

  qStr = QString("update feeds set unread='%1' where id=='%2'").
      arg(unreadCount).arg(parseFeedId);
  q.exec(qStr);

  int newCount = 0;
  qStr = QString("select count(new) from feed_%1 where new==1").
      arg(parseFeedId);
  q.exec(qStr);
  if (q.next()) newCount = q.value(0).toInt();

  qStr = QString("update feeds set newCount='%1' where id=='%2'").
      arg(newCount).arg(parseFeedId);
  q.exec(qStr);

  QModelIndex index = feedsView_->currentIndex();

  // ���� ��������� ��������������� �����, ������� �� ���
  if (parseFeedId ==
      feedsModel_->index(index.row(), feedsModel_->fieldIndex("id")).data().toInt()) {
    slotFeedsTreeClicked(feedsModel_->index(index.row(), 0));
  }
  // ����� ��������� ������ ����
  else {
    feedsModel_->select();
    feedsView_->setCurrentIndex(index);
  }
}

/*! \brief ��������� ������� � ������ ���� ************************************/
void RSSListing::slotFeedsTreeClicked(QModelIndex index)
{
  static QModelIndex indexOld = index;
  if (index.row() != indexOld.row()) {
    slotSetAllRead();
  }
  indexOld = index;
  setFeedsFilter(feedsFilterGroup_->checkedAction());
  bool initNo = false;
  if (newsModel_->columnCount() == 0) initNo = true;
  newsModel_->setTable(QString("feed_%1").arg(feedsModel_->index(index.row(), 0).data().toString()));
  newsModel_->select();
  setNewsFilter(newsFilterGroup_->checkedAction());

  newsHeader_->overload();

  if (initNo) {
    newsHeader_->initColumns();
    newsHeader_->createMenu();
  }

  int row = -1;
  for (int i = 0; i < newsModel_->rowCount(); i++) {
    if (newsModel_->index(i, newsModel_->fieldIndex("id")).data(Qt::EditRole).toInt() ==
        feedsModel_->index(index.row(), feedsModel_->fieldIndex("currentNews")).data().toInt()) {
      row = i;
    }
  }
  newsView_->setCurrentIndex(newsModel_->index(row, 0));
  slotNewsViewClicked(newsModel_->index(row, 0));

  newsTitleLabel_->setText(feedsModel_->index(index.row(), 1).data().toString());
}

/*! \brief ������ ���������� ����� ********************************************/
void RSSListing::getFeed(QString urlString)
{
  persistentUpdateThread_->getUrl(urlString);

  progressBar_->setValue(progressBar_->minimum());
  progressBar_->show();
  QTimer::singleShot(150, this, SLOT(slotProgressBarUpdate()));
}

/*! \brief ��������� ������� � ������ �������� ********************************/
void RSSListing::slotNewsViewClicked(QModelIndex index)
{
  static QModelIndex indexOld = index;
  if (!index.isValid()) {
    webView_->setHtml("");
    webPanel_->hide();
    slotUpdateStatus();  // ����������, ����� ������� ������ �����, �� ������� � ��� �� �������
    return;
  }

  webPanel_->show();

  QModelIndex indexNew = index;
  if (!((index.row() == indexOld.row()) &&
         newsModel_->index(index.row(), newsModel_->fieldIndex("read")).data(Qt::EditRole).toInt() == 1)) {
    updateWebView(index);
    slotSetItemRead(index, 1);

    QSqlQuery q(db_);
    int id = newsModel_->index(index.row(), 0).
        data(Qt::EditRole).toInt();
    QString qStr = QString("update feeds set currentNews='%1' where id=='%2'").
        arg(id).arg(newsModel_->tableName().remove("feed_"));
    q.exec(qStr);
  }
  indexOld = indexNew;
}

/*! \brief ��������� ������ Up/Down � ������ ���� *****************************/
void RSSListing::slotFeedsTreeKeyUpDownPressed()
{
  slotFeedsTreeClicked(feedsView_->currentIndex());
}

/*! \brief ��������� ������ Up/Down � ������ �������� *************************/
void RSSListing::slotNewsKeyUpDownPressed()
{
  slotNewsViewClicked(newsView_->currentIndex());
}

/*! \brief ����� ���� �������� ************************************************/
void RSSListing::showOptionDlg()
{
  OptionsDialog *optionsDialog = new OptionsDialog(this);
  optionsDialog->setWindowTitle(tr("Options"));
  optionsDialog->restoreGeometry(settings_->value("options/geometry").toByteArray());
  optionsDialog->setProxy(networkProxy_);
  int result = optionsDialog->exec();
  settings_->setValue("options/geometry", optionsDialog->saveGeometry());

  if (result == QDialog::Rejected) return;

  networkProxy_ = optionsDialog->proxy();
  persistentUpdateThread_->setProxy(networkProxy_);
}

/*! \brief ��������� ��������� ���������� �� ���������� ����� ��������� *******/
void RSSListing::receiveMessage(const QString& message)
{
  qDebug() << QString("Received message: '%1'").arg(message);
  if (!message.isEmpty()){
    QStringList params = message.split('\n');
    foreach (QString param, params) {
      if (param == "--show") slotShowWindows();
      if (param == "--exit") slotClose();
    }
  }
}

/*! \brief �������� ���� ���� *************************************************/
void RSSListing::createTrayMenu()
{
  trayMenu_ = new QMenu(this);
  QAction *showWindowAct_ = new QAction(tr("Show window"), this);
  connect(showWindowAct_, SIGNAL(triggered()), this, SLOT(slotShowWindows()));
  QFont font_ = showWindowAct_->font();
  font_.setBold(true);
  showWindowAct_->setFont(font_);
  trayMenu_->addAction(showWindowAct_);
  trayMenu_->addAction(updateAllFeedsAct_);
  trayMenu_->addSeparator();

  trayMenu_->addAction(exitAct_);
  traySystem->setContextMenu(trayMenu_);
}

/*! \brief ������������ ������ ************************************************/
void RSSListing::myEmptyWorkingSet()
{
  EmptyWorkingSet(GetCurrentProcess());
}

/*! \brief ���������� ����� (��������) ****************************************/
void RSSListing::slotGetFeed()
{
  progressBar_->setMaximum(1);
  getFeed(feedsModel_->record(feedsView_->currentIndex().row()).
      field("xmlurl").value().toString());
}

/*! \brief ���������� ����� (��������) ****************************************/
void RSSListing::slotGetAllFeeds()
{
  updateAllFeedsAct_->setEnabled(false);

  int feedCount = 0;

  QSqlQuery q(db_);
  q.exec("select xmlurl from feeds where xmlurl is not null");
  while (q.next()) {
    getFeed(q.record().value(0).toString());
    ++feedCount;
  }

  if (0 == feedCount) {
    updateAllFeedsAct_->setEnabled(true);
    return;
  }

  progressBar_->setMaximum(feedCount-1);
}

void RSSListing::slotProgressBarUpdate()
{
  progressBar_->update();

  if (progressBar_->isVisible())
    QTimer::singleShot(150, this, SLOT(slotProgressBarUpdate()));
}

void RSSListing::slotVisibledFeedsDock()
{
  feedsDock_->setVisible(!feedsDock_->isVisible());
  if (newsDockArea_ == feedsDockArea_)
    newsDock_->setVisible(feedsDock_->isVisible());
}

void RSSListing::slotDockLocationChanged(Qt::DockWidgetArea area)
{
  if (area == Qt::LeftDockWidgetArea) {
    pushButtonNull_->setIcon(QIcon(":/images/images/triangleL.png"));
    toolBarNull_->show();
    addToolBar(Qt::LeftToolBarArea, toolBarNull_);
  } else if (area == Qt::RightDockWidgetArea) {
    toolBarNull_->show();
    pushButtonNull_->setIcon(QIcon(":/images/images/triangleR.png"));
    addToolBar(Qt::RightToolBarArea, toolBarNull_);
  } else {
    toolBarNull_->hide();
  }
}

void RSSListing::slotSetItemRead(QModelIndex index, int read)
{
  if (!index.isValid()) return;

  QModelIndex curIndex = newsView_->currentIndex();
  if (newsModel_->index(index.row(), newsModel_->fieldIndex("new")).data(Qt::EditRole).toInt() == 1) {
    newsModel_->setData(
        newsModel_->index(index.row(), newsModel_->fieldIndex("new")),
        0);
//    QSqlQuery q(db_);
//    q.exec(QString("update %1 set new=0 where id=='%2'").
//        arg(newsModel_->tableName()).
//        arg(newsModel_->index(index.row(), newsModel_->fieldIndex("id")).data().toInt()));
//    qDebug() << q.lastQuery() << q.lastError();
  }
  if (newsModel_->index(index.row(), newsModel_->fieldIndex("read")).data(Qt::EditRole).toInt() != read) {
    newsModel_->setData(
        newsModel_->index(index.row(), newsModel_->fieldIndex("read")),
        read);
//    QSqlQuery q(db_);
//    q.exec(QString("update %1 set read='%2' where id=='%3'").
//        arg(newsModel_->tableName()).arg(read).
//        arg(newsModel_->index(index.row(), newsModel_->fieldIndex("id")).data().toInt()));
//    qDebug() << q.lastQuery() << q.lastError();
  }
  newsView_->setCurrentIndex(curIndex);
  slotUpdateStatus();
}

void RSSListing::markNewsRead()
{
  QModelIndex index = newsView_->currentIndex();
  if (newsModel_->index(index.row(), newsModel_->fieldIndex("read")).data(Qt::EditRole).toInt() == 0) {
    slotSetItemRead(index, 1);
  } else {
    slotSetItemRead(index, 0);
  }
}

void RSSListing::markAllNewsRead()
{
  QString qStr = QString("update %1 set read=1 WHERE read=0").
      arg(newsModel_->tableName());
  QSqlQuery q(db_);
  q.exec(qStr);
  qStr = QString("UPDATE %1 SET new=0 WHERE new=1").
      arg(newsModel_->tableName());
  q.exec(qStr);
  newsModel_->select();
  setNewsFilter(newsFilterGroup_->checkedAction());
  slotUpdateStatus();
}

/*! \brief ������� ��������
 *
 * ������� ���� �������� � ����. (50��)
 * ������� ���� �� ����������� �������� � ����. (50��)
 * ������� ���� ����� �������� � ����. (50��)
 * ������ ���� ������ � ������� ���� (100��)
 * ����� ���� ������ � ��������� ������
 */
void RSSListing::slotUpdateStatus()
{
  QString qStr;

  int allCount = 0;
  qStr = QString("select count(id) from %1 where deleted=0").
      arg(newsModel_->tableName());
  QSqlQuery q(db_);
  q.exec(qStr);
  if (q.next()) allCount = q.value(0).toInt();

  int unreadCount = 0;
  qStr = QString("select count(read) from %1 where read==0").
      arg(newsModel_->tableName());
  q.exec(qStr);
  if (q.next()) unreadCount = q.value(0).toInt();

  int newCount = 0;
  qStr = QString("select count(new) from %1 where new==1").
      arg(newsModel_->tableName());
  q.exec(qStr);
  if (q.next()) newCount = q.value(0).toInt();

  qStr = QString("update feeds set unread='%1', newCount='%2' where id=='%3'").
      arg(unreadCount).arg(newCount).
      arg(newsModel_->tableName().remove("feed_"));
  q.exec(qStr);

  QModelIndex index = feedsView_->currentIndex();
  feedsModel_->select();
  feedsView_->setCurrentIndex(index);

  statusUnread_->setText(QString(tr(" Unread: %1 ")).arg(unreadCount));

  statusAll_->setText(QString(tr(" All: %1 ")).arg(allCount));
}

void RSSListing::slotLoadStarted()
{
  if (newsView_->currentIndex().isValid()) {
    webViewProgress_->setValue(0);
    webViewProgress_->show();
  }
}

void RSSListing::slotLoadFinished(bool ok)
{
  if (!ok) statusBar()->showMessage(tr("Error loading to WebVeiw"), 3000);
  webViewProgress_->hide();
}

void RSSListing::setFeedsFilter(QAction* pAct)
{
  QModelIndex index = feedsView_->currentIndex();

  int id = feedsModel_->index(
        index.row(), feedsModel_->fieldIndex("id")).data(Qt::EditRole).toInt();

  if (pAct->objectName() == "filterFeedsAll_") {
    feedsModel_->setFilter("");
  } else if (pAct->objectName() == "filterFeedsNew_") {
    feedsModel_->setFilter(QString("newCount > 0"));
  } else if (pAct->objectName() == "filterFeedsUnread_") {
    feedsModel_->setFilter(QString("unread > 0"));
  }

  if (pAct->objectName() == "filterFeedsAll_") feedsFilter_->setIcon(QIcon(":/images/filterOff"));
  else feedsFilter_->setIcon(QIcon(":/images/filterOn"));

  int rowFeeds = -1;
  for (int i = 0; i < feedsModel_->rowCount(); i++) {
    if (feedsModel_->index(i, feedsModel_->fieldIndex("id")).data(Qt::EditRole).toInt() == id) {
      rowFeeds = i;
    }
  }
  feedsView_->setCurrentIndex(feedsModel_->index(rowFeeds, 0));
}

void RSSListing::setNewsFilter(QAction* pAct)
{
  QModelIndex index = newsView_->currentIndex();

  int id = newsModel_->index(
        index.row(), newsModel_->fieldIndex("id")).data(Qt::EditRole).toInt();

  QString qStr = QString("UPDATE %1 SET read=2 WHERE read=1").
      arg(newsModel_->tableName());
  QSqlQuery q(db_);
  q.exec(qStr);

  if (pAct->objectName() == "filterNewsAll_") {
    newsModel_->setFilter("deleted = 0");
  } else if (pAct->objectName() == "filterNewsNew_") {
    newsModel_->setFilter(QString("new = 1 AND deleted = 0"));
  } else if (pAct->objectName() == "filterNewsUnread_") {
    newsModel_->setFilter(QString("read < 2 AND deleted = 0"));
  } else if (pAct->objectName() == "filterNewsStar_") {
    newsModel_->setFilter(QString("sticky = 1 AND deleted = 0"));
  }

  if (pAct->objectName() == "filterNewsAll_") newsFilter_->setIcon(QIcon(":/images/filterOff"));
  else newsFilter_->setIcon(QIcon(":/images/filterOn"));

  int row = -1;
  for (int i = 0; i < newsModel_->rowCount(); i++) {
    if (newsModel_->index(i, newsModel_->fieldIndex("id")).data(Qt::EditRole).toInt() == id) {
      row = i;
    }
  }
  newsView_->setCurrentIndex(newsModel_->index(row, 0));
  if (row == -1) webView_->setHtml("");
}

void RSSListing::slotFeedsDockLocationChanged(Qt::DockWidgetArea area)
{
  feedsDockArea_ = area;
}

void RSSListing::slotNewsDockLocationChanged(Qt::DockWidgetArea area)
{
  newsDockArea_ = area;
}

void RSSListing::slotNewsViewDoubleClicked(QModelIndex index)
{
  QDesktopServices::openUrl(
        QUrl(newsModel_->index(index.row(), newsModel_->fieldIndex("link_href")).data(Qt::EditRole).toString()));
}

void RSSListing::slotSetAllRead()
{
  QString qStr = QString("UPDATE %1 SET read=2 WHERE read=1").
      arg(newsModel_->tableName());
  QSqlQuery q(db_);
  q.exec(qStr);
  qStr = QString("UPDATE %1 SET new=0 WHERE new=1").
      arg(newsModel_->tableName());
  q.exec(qStr);

  qStr = QString("update feeds set newCount=0 where id=='%2'").
      arg(newsModel_->tableName().remove("feed_"));
  q.exec(qStr);
}

void RSSListing::slotShowAboutDlg()
{
  AboutDialog *aboutDialog = new AboutDialog(this);
  aboutDialog->exec();
}

void RSSListing::deleteNews()
{
  QModelIndex index = newsView_->currentIndex();
  if (!index.isValid()) return;

  newsModel_->setData(
      newsModel_->index(index.row(), newsModel_->fieldIndex("read")),
      1);
  if (newsModel_->index(index.row(), newsModel_->fieldIndex("deleted")).data(Qt::EditRole).toInt() == 0) {
    newsModel_->setData(
        newsModel_->index(index.row(), newsModel_->fieldIndex("deleted")),
        1);
  }
  if (index.row() == newsModel_->rowCount())
    index = newsModel_->index(index.row()-1, index.column());
  newsView_->setCurrentIndex(index);
  slotNewsViewClicked(index);
  slotUpdateStatus();
}

void RSSListing::createMenuNews()
{
  newsContextMenu_ = new QMenu(this);
  newsContextMenu_->addAction(openInBrowserAct_);
  newsContextMenu_->addSeparator();
  newsContextMenu_->addAction(markNewsRead_);
  newsContextMenu_->addAction(markAllNewsRead_);
  newsContextMenu_->addSeparator();
  newsContextMenu_->addAction(markStarAct_);
  newsContextMenu_->addSeparator();
  newsContextMenu_->addAction(updateFeedAct_);
  newsContextMenu_->addSeparator();
  newsContextMenu_->addAction(deleteNewsAct_);
}

void RSSListing::showContextMenuNews(const QPoint &p)
{
  if (newsView_->currentIndex().isValid())
    newsContextMenu_->popup(newsView_->viewport()->mapToGlobal(p));
}

void RSSListing::openInBrowserNews()
{
  slotNewsViewDoubleClicked(newsView_->currentIndex());
}

void RSSListing::slotSetItemStar(QModelIndex index, int sticky)
{
  if (!index.isValid()) return;

  QModelIndex curIndex = newsView_->currentIndex();
  newsModel_->setData(
      newsModel_->index(index.row(), newsModel_->fieldIndex("sticky")),
      sticky);
  newsView_->setCurrentIndex(curIndex);
}

void RSSListing::markNewsStar()
{
  QModelIndex index = newsView_->currentIndex();
  if (newsModel_->index(index.row(), newsModel_->fieldIndex("sticky")).data(Qt::EditRole).toInt() == 0) {
    slotSetItemStar(index, 1);
  } else {
    slotSetItemStar(index, 0);
  }
}

void RSSListing::createMenuFeed()
{
  feedContextMenu_ = new QMenu(this);
  feedContextMenu_->addAction(addFeedAct_);
  feedContextMenu_->addSeparator();
  feedContextMenu_->addAction(markFeedRead_);
  feedContextMenu_->addSeparator();
  feedContextMenu_->addAction(updateFeedAct_);
  feedContextMenu_->addSeparator();
  feedContextMenu_->addAction(deleteFeedAct_);
  feedContextMenu_->addSeparator();
  feedContextMenu_->addAction(feedProperties_);
}

void RSSListing::showContextMenuFeed(const QPoint &p)
{
  if (feedsView_->currentIndex().isValid())
    feedContextMenu_->popup(feedsView_->viewport()->mapToGlobal(p));
}

void RSSListing::slotLinkClicked(QUrl url)
{
  QDesktopServices::openUrl(url);
}

void RSSListing::setAutoLoadImages(bool checked)
{
  if (checked) {
    autoLoadImagesToggle_->setIcon(QIcon(":/images/imagesOn"));
  } else {
    autoLoadImagesToggle_->setIcon(QIcon(":/images/imagesOff"));
  }
  webView_->settings()->setAttribute(QWebSettings::AutoLoadImages, checked);
  updateWebView(newsView_->currentIndex());
}

void RSSListing::loadSettingsFeeds()
{
  autoLoadImagesToggle_->setChecked(settings_->value("Settings/autoLoadImages", false).toBool());
  setAutoLoadImages(autoLoadImagesToggle_->isChecked());

  QString filterName = settings_->value("feedSettings/filterName", "filterFeedsAll_").toString();
  QList<QAction*> listActions = feedsFilterGroup_->actions();
  foreach(QAction *action, listActions) {
    if (action->objectName() == filterName) {
      action->setChecked(true);
      break;
    }
  }
  filterName = settings_->value("newsSettings/filterName", "filterNewsAll_").toString();
  listActions = newsFilterGroup_->actions();
  foreach(QAction *action, listActions) {
    if (action->objectName() == filterName) {
      action->setChecked(true);
      break;
    }
  }

  setFeedsFilter(feedsFilterGroup_->checkedAction());
  int id = settings_->value("feedSettings/currentId", 0).toInt();
  int row = -1;
  for (int i = 0; i < feedsModel_->rowCount(); i++) {
    if (feedsModel_->index(i, 0).data().toInt() == id) {
      row = i;
    }
  }

  feedsView_->setCurrentIndex(feedsModel_->index(row, 0));
  slotFeedsTreeClicked(feedsModel_->index(row, 0));  // �������� ��������
}

void RSSListing::updateWebView(QModelIndex index)
{
  if (!index.isValid()) {
    webView_->setHtml("");
    webPanel_->hide();
    return;
  }

  webPanel_->show();

  QString titleString = newsModel_->record(index.row()).field("title").value().toString();
  titleString = webPanelTitle_->fontMetrics().elidedText(
      titleString, Qt::ElideRight, webPanelTitle_->width());
  QString panelTitleString = QString("<a href='%1'>%2</a>").
      arg(newsModel_->record(index.row()).field("link_href").value().toString()).
      arg(titleString);
  webPanelTitle_->setText(panelTitleString);

  // ��������� ������ ������ �� ������ �������
  QString authorString;
  QString authorName = newsModel_->record(index.row()).field("author_name").value().toString();
  QString authorEmail = newsModel_->record(index.row()).field("author_email").value().toString();
  QString authorUri = newsModel_->record(index.row()).field("author_uri").value().toString();
  qDebug() << "author_news:" << authorName << authorEmail << authorUri;
  authorString = authorName;
  if (!authorEmail.isEmpty()) authorString.append(QString(" <a href='mailto:%1'>e-mail</a>").arg(authorEmail));
  if (!authorUri.isEmpty())   authorString.append(QString(" <a href='%1'>page</a>").arg(authorUri));

  // ���� ����� ������� ���, ��������� ������ ������ �� ������ �����
  // @NOTE(arhohryakov:2012.01.03) ����� ������ �� �������� ����, �.�. ���
  //   ������� ����������� ������ � ����
  if (authorString.isEmpty()) {
    authorName = feedsModel_->record(feedsView_->currentIndex().row()).field("author_name").value().toString();
    authorEmail = feedsModel_->record(feedsView_->currentIndex().row()).field("author_email").value().toString();
    authorUri = feedsModel_->record(feedsView_->currentIndex().row()).field("author_uri").value().toString();
    qDebug() << "author_feed:" << authorName << authorEmail << authorUri;
    authorString = authorName;
    if (!authorEmail.isEmpty()) authorString.append(QString(" <a href='mailto:%1'>e-mail</a>").arg(authorEmail));
    if (!authorUri.isEmpty())   authorString.append(QString(" <a href='%1'>page</a>").arg(authorUri));
  }

  webPanelAuthor_->setText(authorString);
  webPanelAuthorLabel_->setVisible(!authorString.isEmpty());
  webPanelAuthor_->setVisible(!authorString.isEmpty());

  QString content = newsModel_->record(index.row()).field("content").value().toString();
  if (content.isEmpty()) {
    webView_->setHtml(
          newsModel_->record(index.row()).field("description").value().toString());
    qDebug() << "setHtml : description";
  }
  else {
    webView_->setHtml(content);
    qDebug() << "setHtml : content";
  }
}

void RSSListing::slotFeedsFilter()
{
  static QAction *action = NULL;

  if (feedsFilterGroup_->checkedAction()->objectName() == "filterFeedsAll_") {
    if (action != NULL) {
      action->setChecked(true);
      setFeedsFilter(action);
    } else {
      feedsFilterMenu_->popup(cursor().pos());
    }
  } else {
    action = feedsFilterGroup_->checkedAction();
    filterFeedsAll_->setChecked(true);
    setFeedsFilter(filterFeedsAll_);
  }
}

void RSSListing::slotNewsFilter()
{
  static QAction *action = NULL;

  if (newsFilterGroup_->checkedAction()->objectName() == "filterNewsAll_") {
    if (action != NULL) {
      action->setChecked(true);
      setNewsFilter(action);
    } else {
      newsFilterMenu_->popup(cursor().pos());
    }
  } else {
    action = newsFilterGroup_->checkedAction();
    filterNewsAll_->setChecked(true);
    setNewsFilter(filterNewsAll_);
  }
}
