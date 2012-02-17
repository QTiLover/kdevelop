/*  This file is part of KDevelop
    Copyright 2012 Miha Čančula <miha@noughmad.eu>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef KDEVELOP_ITESTSUITE_H
#define KDEVELOP_ITESTSUITE_H

#include "interfacesexport.h"

#include <QtCore/QStringList>
#include <QtCore/QMap>

class KJob;
class KUrl;

namespace KDevelop {

class IProject;
class ILaunchConfiguration;

struct KDEVPLATFORMINTERFACES_EXPORT TestResult
{
    enum TestCaseResult
    {
        NotRun,
        Skipped,
        Passed,
        Failed
    };
    QMap<QString, TestCaseResult> testCaseResults;
};

class KDEVPLATFORMINTERFACES_EXPORT ITestSuite
{

public:
    virtual ~ITestSuite();

    virtual QString name() const = 0;
    virtual QStringList cases() const = 0;
    virtual KUrl url() const = 0;
    virtual IProject* project() const = 0;

    virtual KJob* launchAllCases() const = 0;
    virtual KJob* launchCases(const QStringList& testCases) const = 0;
    virtual KJob* launchCase(const QString& testCase) const = 0;
    
    virtual TestResult result() const = 0;
};

}

#endif // KDEVELOP_ITESTSUITE_H
