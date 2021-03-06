/* 
   Copyright 2007 David Nolden <david.nolden.kdevelop@art-master.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef __STRINGHELPERS_H__
#define __STRINGHELPERS_H__

#include <QString>

namespace Utils {

/**
 * Copied from kdevelop-3.4, should be redone
 * @param index should be the index BEHIND the expression
 * */
int expressionAt( const QString& contents, int index );

QString quoteExpression(const QString& expr);

QString unquoteExpression(const QString& expr);

/**
 * Qoute the string, using quoteCh
 */
QString quote(const QString& str, QChar quoteCh = QLatin1Char('"'));

/**
 * Unquote and optionally unescape unicode escape sequence.
 * Handle escape sequence
 *     '\\' '\\'                      -> '\\'
 *     '\\' quoteCh                   -> quoteCh
 *     '\\' 'u' 'N' 'N' 'N' 'N'       -> '\uNNNN'
 *     '\\' 'x''N''N'                 -> '\xNN'
 */
QString unquote(const QString& str, bool unescapeUnicode = false, QChar quoteCh = QLatin1Char('"'));

} // end of namespace Utils

#endif // __STRINGHELPERS_H__
