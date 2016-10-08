#include <QMessageBox>
#include <QMenu>
#include "tag-tab.h"
#include "ui_tag-tab.h"
#include "ui/QBouton.h"
#include "viewer/zoomwindow.h"
#include "searchwindow.h"


tagTab::tagTab(int id, QMap<QString,Site*> *sites, Profile *profile, mainWindow *parent)
	: searchTab(id, sites, profile, parent), ui(new Ui::tagTab), m_id(id), m_favorites(profile->getFavorites()), m_pagemax(-1), m_lastTags(QString()), m_sized(false), m_stop(true)
{
	ui->setupUi(this);
	ui->widgetMeant->hide();
	setAttribute(Qt::WA_DeleteOnClose);

	// UI members for SearchTab class
	ui_spinPage = ui->spinPage;
	ui_spinImagesPerPage = ui->spinImagesPerPage;
	ui_spinColumns = ui->spinColumns;
	ui_widgetMeant = ui->widgetMeant;
	ui_labelMeant = ui->labelMeant;
	ui_layoutResults = ui->layoutResults;
	ui_layoutSourcesList = ui->layoutSourcesList;
	ui_buttonHistoryBack = ui->buttonHistoryBack;
	ui_buttonHistoryNext = ui->buttonHistoryNext;

	// Search fields
	m_search = new TextEdit(m_profile, this);
	m_postFiltering = new TextEdit(m_profile, this);
		if (m_settings->value("autocompletion", true).toBool())
		{
			QCompleter *completer = new QCompleter(m_completion, this);
				completer->setCaseSensitivity(Qt::CaseInsensitive);
			m_search->setCompleter(completer);
			m_postFiltering->setCompleter(completer);
		}
		connect(m_search, SIGNAL(returnPressed()), this, SLOT(load()));
		connect(m_search, SIGNAL(favoritesChanged()), m_parent, SLOT(updateFavorites()));
		connect(m_search, SIGNAL(favoritesChanged()), m_parent, SLOT(updateFavoritesDock()));
		connect(m_search, SIGNAL(kflChanged()), m_parent, SLOT(updateKeepForLater()));
		connect(m_postFiltering, SIGNAL(returnPressed()), this, SLOT(load()));
		connect(ui->labelMeant, SIGNAL(linkActivated(QString)), this, SLOT(setTags(QString)));
		ui->layoutFields->insertWidget(1, m_search, 1);
		ui->layoutPlus->addWidget(m_postFiltering, 1, 1, 1, 3);

	setSelectedSources(m_settings);

	// Half MD5 field
	ui->lineMd5->setEnabled(m_settings->value("enable_md5_field", false).toBool());
	ui->lineMd5->setVisible(m_settings->value("enable_md5_field", false).toBool());

	// Others
	ui->checkMergeResults->setChecked(m_settings->value("mergeresults", false).toBool());
	optionsChanged();
	ui->widgetPlus->hide();
	setWindowIcon(QIcon());
	updateCheckboxes();
	m_search->setFocus();
}

tagTab::~tagTab()
{
	close();
	delete ui;
}

void tagTab::on_buttonSearch_clicked()
{
	SearchWindow *sw = new SearchWindow(m_search->toPlainText(), m_profile, this);
	connect(sw, SIGNAL(accepted(QString)), this, SLOT(setTags(QString)));
	sw->show();
}

void tagTab::closeEvent(QCloseEvent *e)
{
	m_settings->setValue("mergeresults", ui->checkMergeResults->isChecked());
	m_settings->sync();

	qDeleteAll(m_pages);
	m_pages.clear();
	m_images.clear();
	qDeleteAll(m_checkboxes);
	m_checkboxes.clear();
	for (int i = 0; i < m_layouts.size(); i++)
	{ clearLayout(m_layouts[i]); }
	qDeleteAll(m_layouts);
	m_layouts.clear();
	m_boutons.clear();

	emit closed(this);
	e->accept();
}



void tagTab::optionsChanged()
{
	log(tr("Mise à jour des options de l'onglet \"%1\".").arg(windowTitle()));
	ui->retranslateUi(this);
	ui->spinImagesPerPage->setValue(m_settings->value("limit", 20).toInt());
	ui->spinColumns->setValue(m_settings->value("columns", 1).toInt());
	/*QPalette p = ui->widgetResults->palette();
	p.setColor(ui->widgetResults->backgroundRole(), QColor(m_settings->value("serverBorderColor", "#000000").toString()));
	ui->widgetResults->setPalette(p);*/
	ui->layoutResults->setHorizontalSpacing(m_settings->value("Margins/main", 10).toInt());
}



void tagTab::load()
{
	log(tr("Chargement des résultats..."));

	ui->buttonGetAll->setEnabled(false);
	ui->buttonGetpage->setEnabled(false);
	ui->buttonGetSel->setEnabled(false);

	m_stop = true;
	m_parent->setWiki("");
	m_pagemax = -1;

	QString search = m_search->toPlainText();
	if (!ui->lineMd5->text().isEmpty())
	{
		if (!search.isEmpty())
		{ search += ' '; }

		search += "md5:" + ui->lineMd5->text();
	}

	if (!m_from_history)
	{ addHistory(search, ui->spinPage->value(), ui->spinImagesPerPage->value(), ui->spinColumns->value()); }
	m_from_history = false;

	if (search != m_lastTags && !m_lastTags.isNull() && m_history_cursor == m_history.size() - 1)
	{ ui->spinPage->setValue(1); }
	m_lastTags = search;

	ui->widgetMeant->hide();
	ui->buttonFirstPage->setEnabled(ui->spinPage->value() > 1);
	ui->buttonPreviousPage->setEnabled(ui->spinPage->value() > 1);

	// Clear results layout
	for (int i = 0; i < m_layouts.size(); i++)
	{ clearLayout(m_layouts[i]); }
	qDeleteAll(m_layouts);
	m_layouts.clear();
	m_boutons.clear();
	ui->verticalLayout->removeWidget(ui->widgetResults);
	ui->widgetResults->deleteLater();
	ui->widgetResults = new QWidget(this);
	ui->verticalLayout->insertWidget(0, ui->widgetResults);

	clearLayout(ui->layoutResults);
	ui->layoutResults->deleteLater();
	ui->layoutResults = new QGridLayout(ui->widgetResults);
	ui_layoutResults = ui->layoutResults;

	setWindowTitle(search.isEmpty() ? tr("Recherche") : search.replace("&", "&&"));
	emit titleChanged(this);
	m_tags.clear();
	m_parent->setTags(m_tags, this);
	for (int i = 0; i < m_pages.size(); i++)
	{
		m_pages.value(m_pages.keys().at(i))->abort();
		m_pages.value(m_pages.keys().at(i))->abortTags();
	}
	//qDeleteAll(m_pages);
	m_pages.clear();
	for (int i = 0; i < m_images.size(); i++)
	{ m_images.at(i)->abortPreview(); }
	//qDeleteAll(m_images);
	m_images.clear();

	for (int i = 0; i < m_selectedSources.size(); i++)
	{
		if (m_checkboxes.at(i)->isChecked())
		{
			// Get the search values
			QStringList tags = search.trimmed().split(" ", QString::SkipEmptyParts);
			tags.append(m_settings->value("add").toString().trimmed().split(" ", QString::SkipEmptyParts));
			int perpage = ui->spinImagesPerPage->value();

			// Load results
			Page *page = new Page(m_sites->value(m_sites->keys().at(i)), m_sites->values(), tags, ui->spinPage->value(), perpage, m_postFiltering->toPlainText().split(" ", QString::SkipEmptyParts), false, this, 0, m_lastPage, m_lastPageMinId, m_lastPageMaxId);
			log(tr("Chargement de la page <a href=\"%1\">%1</a>").arg(page->url().toString().toHtmlEscaped()));
			connect(page, SIGNAL(finishedLoading(Page*)), this, SLOT(finishedLoading(Page*)));
			connect(page, SIGNAL(failedLoading(Page*)), this, SLOT(failedLoading(Page*)));
			m_pages.insert(page->website(), page);

			// Setup the layout
			QGridLayout *l = new QGridLayout;
			l->setHorizontalSpacing(m_settings->value("Margins/horizontal", 6).toInt());
			l->setVerticalSpacing(m_settings->value("Margins/vertical", 6).toInt());
			m_layouts.append(l);

			// Load tags if necessary
			m_stop = false;
			if (m_settings->value("useregexfortags", true).toBool())
			{
				connect(page, SIGNAL(finishedLoadingTags(Page*)), this, SLOT(finishedLoadingTags(Page*)));
				page->loadTags();
			}

			// Start loading
			page->load();
		}
	}
	if (ui->checkMergeResults->isChecked() && m_layouts.size() > 0)
	{ ui->layoutResults->addLayout(m_layouts[0], 0, 0); }
	m_page = 0;

	emit changed(this);
}

void tagTab::finishedLoading(Page* page)
{
	if (m_stop)
		return;

	log(tr("Réception de la page <a href=\"%1\">%1</a>").arg(page->url().toString().toHtmlEscaped()));

	m_lastPage = page->page();
	m_lastPageMinId = page->minId();
	m_lastPageMaxId = page->maxId();
	QList<Image*> imgs = page->images();
	m_images.append(imgs);

	int maxpage = page->pagesCount();
	if (maxpage < m_pagemax || m_pagemax == -1)
		m_pagemax = maxpage;
	ui->buttonNextPage->setEnabled(maxpage > ui->spinPage->value() || page->imagesCount() == -1 || page->pagesCount() == -1 || (page->imagesCount() == 0 && page->images().count() > 0));
	ui->buttonLastPage->setEnabled(maxpage > ui->spinPage->value() || page->imagesCount() == -1 || page->pagesCount() == -1);

	if (!ui->checkMergeResults->isChecked())
	{ addResultsPage(page, imgs); }

	if (!m_settings->value("useregexfortags", true).toBool())
	{ setTagsFromPages(m_pages); }

	postLoading(page);
}

void tagTab::failedLoading(Page *page)
{
	if (ui->checkMergeResults->isChecked())
	{
		postLoading(page);
	}
}

void tagTab::postLoading(Page *page)
{
	QList<Image*> imgs;

	m_page++;
	if (ui->checkMergeResults->isChecked())
	{
		if (m_page != m_pages.size())
			return;

		QStringList md5s;
		for (int i = 0; i < m_images.count(); i++)
		{
			QString md5 = m_images.at(i)->md5();
			if (md5.isEmpty())
				continue;

			if (md5s.contains(md5))
				m_images.removeAt(i--);
			else
				md5s.append(md5);
		}

		imgs = m_images;
	}
	else
	{ imgs = page->images(); }

	// Loading images thumbnails
	for (int i = 0; i < imgs.count(); i++)
	{
		QStringList detected;
		Image *img = imgs.at(i);
		QStringList tags = m_search->toPlainText().toLower().split(' ');
		QList<QChar> modifiers = QList<QChar>() << '~';
		for (int r = 0; r < tags.size(); r++)
		{
			if (modifiers.contains(tags[r][0]))
			{ tags[r] = tags[r].right(tags[r].size()-1); }
		}
		if (!m_settings->value("blacklistedtags").toString().isEmpty())
		{ detected = img->blacklisted(m_settings->value("blacklistedtags").toString().toLower().split(" ")); }
		if (!detected.isEmpty() && m_settings->value("hideblacklisted", false).toBool())
		{ log(tr("Image #%1 ignorée. Raison : %2.").arg(i).arg("\""+detected.join(", ")+"\""));; }
		else
		{
			connect(img, SIGNAL(finishedLoadingPreview(Image*)), this, SLOT(finishedLoadingPreview(Image*)));
			img->loadPreview();
		}
	}

	ui->buttonGetAll->setDisabled(m_images.empty());
	ui->buttonGetpage->setDisabled(m_images.empty());
	ui->buttonGetSel->setDisabled(m_images.empty());
}


void tagTab::finishedLoadingTags(Page *page)
{
	// Tags for this page
	QList<Tag> taglist;
	QStringList tagsGot;
	QStringList toAdd;
	for (int i = 0; i < m_pages.count(); i++)
	{
		QList<Tag> tags = m_pages.value(m_pages.keys().at(i))->tags();
		for (int t = 0; t < tags.count(); t++)
		{
			if (!tags[t].text().isEmpty())
			{
				if (tags[t].count() >= m_settings->value("tagsautoadd", 10).toInt()
					&& !m_completion.contains(tags[t].text()))
				{
					toAdd.append(tags[t].text());
					m_completion.append(tags[t].text());
				}
				if (tagsGot.contains(tags[t].text()))
				{ taglist[tagsGot.indexOf(tags[t].text())].setCount(taglist[tagsGot.indexOf(tags[t].text())].count()+tags[t].count()); }
				else
				{
					taglist.append(tags[t]);
					tagsGot.append(tags[t].text());
				}
			}
		}
	}
	QFile wordsc(savePath("wordsc.txt"));
	if (wordsc.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
	{
		wordsc.write(QString('\n').toLatin1());
		wordsc.write(toAdd.join(' ').toLatin1());
		wordsc.close();
	}

	// We sort tags by frequency
	qSort(taglist.begin(), taglist.end(), sortByFrequency);

	m_tags = taglist;
	m_parent->setTags(m_tags, this);

	// Wiki
	if (!page->wiki().isEmpty())
	{
		m_wiki = "<style>.title { font-weight: bold; } ul { margin-left: -30px; }</style>"+page->wiki();
		m_parent->setWiki(m_wiki);
	}

	int maxpage = page->pagesCount();
	if (maxpage < m_pagemax || m_pagemax == -1)
	{ m_pagemax = maxpage; }
	ui->buttonNextPage->setEnabled(maxpage > ui->spinPage->value() || page->imagesCount() == -1 || (page->imagesCount() == 0 && page->images().count() > 0));
	ui->buttonLastPage->setEnabled(maxpage > ui->spinPage->value());
}

void tagTab::finishedLoadingPreview(Image *img)
{
	if (m_stop)
	{ return; }

	int position = m_images.indexOf(img), page = 0;
	if (!ui->checkMergeResults->isChecked())
	{
		page = m_pages.values().indexOf(img->page());
		if (page < 0)
		{ return; }
	}
	if (img->previewImage().isNull())
		return;

	QStringList blacklistedtags(m_settings->value("blacklistedtags").toString().split(" "));
	QStringList detected = img->blacklisted(blacklistedtags);
	QStringList whitelistedtags(m_settings->value("whitelistedtags").toString().split(" "));
	QStringList whitelisted = img->blacklisted(whitelistedtags);
	if (!whitelisted.isEmpty() && m_settings->value("whitelist_download", "image").toString() == "page")
	{
		bool download = false;
		if (!detected.isEmpty())
		{
			int reponse = QMessageBox::question(this, tr("Grabber"), tr("Certains tags de l'image sont dans la liste blanche : \"%1\". Cependant, certains dans la liste noire : \"%2\". Voulez-vous la télécharger tout de même ?").arg(whitelisted.join(", "), detected.join(", ")), QMessageBox::Yes | QMessageBox::Open | QMessageBox::No);
			if (reponse == QMessageBox::Yes)
			{ download = true; }
			else if (reponse == QMessageBox::Open)
			{
				zoomWindow *zoom = new zoomWindow(img, img->page()->site(), m_sites, m_profile, m_parent);
				zoom->show();
				connect(zoom, SIGNAL(linkClicked(QString)), this, SLOT(setTags(QString)));
				connect(zoom, SIGNAL(poolClicked(int, QString)), m_parent, SLOT(addPoolTab(int, QString)));
			}
		}
		else
		{ download = true; }

		if (download)
		{
			connect(img, SIGNAL(finishedImage(Image*)), m_parent, SLOT(saveImage(Image*)));
			connect(img, SIGNAL(finishedImage(Image*)), m_parent, SLOT(decreaseDownloads()));

			QString filename = m_settings->value("Save/filename").toString();;
			bool getTags = filename.startsWith("javascript:") || (filename.contains("%filename%") && img->site().contains("Regex/ForceImageUrl"));
			QStringList forbidden = QStringList() << "artist" << "copyright" << "character" << "model" << "general";
			for (int i = 0; i < forbidden.count(); i++)
			{ getTags = getTags || filename.contains("%"+forbidden.at(i)+"%"); }

			if (getTags)
			{
				connect(img, SIGNAL(finishedLoadingTags(Image*)), img, SLOT(loadImage()));
				img->loadDetails();
			}
			else
			{ img->loadImage(); }

			m_parent->increaseDownloads();
		}
	}

	addResultsImage(img, ui->checkMergeResults->isChecked() && !m_images.empty());
}

void tagTab::toggleImage(int id, bool toggle)
{
	if (toggle)
	{ selectImage(m_images.at(id)); }
	else
	{ unselectImage(m_images.at(id)); }
}

void tagTab::setTags(QString tags)
{
	activateWindow();
	m_search->setText(tags);
	load();
}

void tagTab::getPage()
{
	if (m_pages.empty())
		return;
	QStringList actuals, keys = m_sites->keys();
	for (int i = 0; i < m_checkboxes.count(); i++)
	{
		if (m_checkboxes.at(i)->isChecked())
		{ actuals.append(keys.at(i)); }
	}
	bool unloaded = m_settings->value("getunloadedpages", false).toBool();
	for (int i = 0; i < actuals.count(); i++)
	{
		if (m_pages.contains(actuals.at(i)))
		{
			int perpage = unloaded ? ui->spinImagesPerPage->value() : (m_pages.value(actuals.at(i))->images().count() > ui->spinImagesPerPage->value() ? m_pages.value(actuals.at(i))->images().count() : ui->spinImagesPerPage->value());
			if (perpage <= 0 || m_pages.value(actuals.at(i))->images().count() <= 0)
				continue;

			QString search = m_pages.value(actuals.at(i))->search().join(' ');
			emit batchAddGroup(QStringList()
							   << search
							   << QString::number(ui->spinPage->value())
							   << QString::number(perpage)
							   << QString::number(perpage)
							   << m_settings->value("downloadblacklist").toString()
							   << actuals.at(i)
							   << m_settings->value("Save/filename").toString()
							   << m_settings->value("Save/path").toString()
							   << "");
		}
	}
}
void tagTab::getAll()
{
	if (m_pages.empty())
		return;

	QStringList actuals, keys = m_sites->keys();
	for (int i = 0; i < m_checkboxes.count(); i++)
	{
		if (m_checkboxes.at(i)->isChecked())
			actuals.append(keys.at(i));
	}

	for (int i = 0; i < actuals.count(); i++)
	{
		int limit = m_pages.value(actuals.at(i))->highLimit();
		int v1 = qMin((limit > 0 ? limit : 200), qMax(m_pages.value(actuals.at(i))->images().count(), m_pages.value(actuals.at(i))->imagesCount()));
		int v2 = qMax(m_pages.value(actuals.at(i))->images().count(), m_pages.value(actuals.at(i))->imagesCount());
		if (v1 == 0 && v2 == 0)
			continue;

		QString search = m_pages.value(actuals.at(i))->search().join(' ');
		emit batchAddGroup(QStringList()
						   << search
						   << "1"
						   << QString::number(v1)
						   << QString::number(v2)
						   << m_settings->value("downloadblacklist").toString()
						   << actuals.at(i)
						   << m_settings->value("Save/filename").toString()
						   << m_settings->value("Save/path").toString()
						   << "");
	}
}

void tagTab::firstPage()
{
	ui->spinPage->setValue(1);
	load();
}
void tagTab::previousPage()
{
	if (ui->spinPage->value() > 1)
	{
		ui->spinPage->setValue(ui->spinPage->value()-1);
		load();
	}
}
void tagTab::nextPage()
{
	if (ui->spinPage->value() < ui->spinPage->maximum())
	{
		ui->spinPage->setValue(ui->spinPage->value()+1);
		load();
	}
}
void tagTab::lastPage()
{
	ui->spinPage->setValue(m_pagemax);
	load();
}


void tagTab::focusSearch()
{
	m_search->setFocus();
}

void tagTab::setImagesPerPage(int ipp)
{ ui->spinImagesPerPage->setValue(ipp); }
void tagTab::setColumns(int columns)
{ ui->spinColumns->setValue(columns); }
void tagTab::setPostFilter(QString postfilter)
{ m_postFiltering->setText(postfilter); }

int tagTab::imagesPerPage()		{ return ui->spinImagesPerPage->value();	}
int tagTab::columns()			{ return ui->spinColumns->value();			}
QString tagTab::postFilter()	{ return m_postFiltering->toPlainText();	}
QString tagTab::tags()			{ return m_search->toPlainText();			}
QString tagTab::wiki()			{ return m_wiki;							}
