/* KDevelop CMake Support
 *
 * Copyright 2006 Matt Rogers <mattr@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef CMAKEXMLPARSER_H
#define CMAKEXMLPARSER_H

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtXml/QDomElement>

#include "kurl.h"

struct TargetInfo
{
    QString name;
    QString type;
    QStringList sources;
};

struct FolderInfo
{
    QString name;
    QList<FolderInfo> subFolders;
    QStringList includes;
    QStringList defines;
    QList<TargetInfo> targets;
};

struct ProjectInfo
{
    QString name;
    QString root;
    FolderInfo rootFolder;
};

/** This class parses the xml output generated by cmake */
class CMakeXmlParser
{
public:
    CMakeXmlParser() {}
    ~CMakeXmlParser() {}

    ProjectInfo parse( const KUrl& file );

    ProjectInfo parseProject( const QDomDocument& );
    TargetInfo parseTarget( const QDomElement& );
    FolderInfo parseFolder( const QDomElement& );
};

#endif
