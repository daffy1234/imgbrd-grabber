#ifndef TAG_DATABASE_H
#define TAG_DATABASE_H

#include <QString>
#include <QMap>
#include "tag-type.h"
#include "tag.h"


class TagDatabase
{
	public:
		virtual bool load();
		virtual bool save() = 0;
		virtual void setTags(const QList<Tag> &tags) = 0;
		virtual TagType getTagType(QString tag) const = 0;
		QMap<int, TagType> tagTypes() const;

	protected:
		TagDatabase(QString typeFile);

	private:
		void loadTypes(QString filename);

	protected:
		QMap<int, TagType> m_tagTypes;

	private:
		QString m_typeFile;
};

#endif // TAG_DATABASE_H
