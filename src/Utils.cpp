// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on
#include "Utils.h"

#include "compat.h"
#include "fileaccess.h"
#include "TypeUtils.h"

#include <set>
#include <unicode/ucnv.h>
#include <unicode/ucnv_err.h>

#include <QString>
#include <QStringList>
#include <QHash>
#include <QRegularExpression>

/* Split the command line into arguments.
 * Normally split at white space separators except when quoting with " or '.
 * Backslash is treated as meta.
 * Detect parsing errors like unclosed quotes.
 * The first item in the list will be the command itself.
 * Returns the error reason as string or an empty string on success.
 * Eg. >"1" "2"<           => >1<, >2<
 * Eg. >'\'\\'<            => >'\<   backslash is a meta character
 * Eg. > "\\" <            => >\<
 * Eg. >"c:\sed" 's/a/\' /g'<  => >c:\sed<, >s/a/' /g<
 */
QString Utils::getArguments(QString cmd, QString& program, QStringList& args)
{
    program = QString();
    args.clear();
    for(qsizetype i = 0; i < cmd.length(); ++i)
    {
        while(i < cmd.length() && cmd[i].isSpace())
        {
            ++i;
        }
        if(cmd[i] == u'"' || cmd[i] == u'\'') // argument beginning with a quote
        {
            QChar quoteChar = cmd[i];
            ++i;
            qsizetype argStart = i;
            bool bSkip = false;
            while(i < cmd.length() && (cmd[i] != quoteChar || bSkip))
            {
                if(bSkip)
                {
                    bSkip = false;
                    //Don't emulate bash here we are not talking to it.
                    //For us all quotes are the same.
                    if(cmd[i] == u'\\' || cmd[i] == u'\'' || cmd[i] == u'"')
                    {
                        cmd.remove(i - 1, 1); // remove the backslash u'\'
                        continue;
                    }
                }
                else if(cmd[i] == u'\\')
                    bSkip = true;
                ++i;
            }
            if(i < cmd.length())
            {
                args << cmd.mid(argStart, i - argStart);
                if(i + 1 < cmd.length() && !cmd[i + 1].isSpace())
                    return i18n("Expecting space after closing quote.");
            }
            else
                return i18n("Unmatched quote.");
            continue;
        }
        else
        {
            qsizetype argStart = i;
            while(i < cmd.length() && (!cmd[i].isSpace() /*|| bSkip*/))
            {
                if(cmd[i] == u'"' || cmd[i] == u'\'')
                    return i18n("Unexpected quote character within argument.");
                ++i;
            }
            args << cmd.mid(argStart, i - argStart);
        }
    }
    if(args.isEmpty())
        return i18n("No program specified.");
    else
    {
        program = args[0];
        args.pop_front();
    }
    return QString();
}

bool Utils::wildcardMultiMatch(const QString& wildcard, const QString& testString, bool bCaseSensitive)
{
    static QHash<QString, QRegularExpression> s_patternMap;

    const QStringList regExpList = wildcard.split(QChar(u';'));

    for(const QString& regExp : regExpList)
    {
        QHash<QString, QRegularExpression>::iterator patIt = s_patternMap.find(regExp);
        if(patIt == s_patternMap.end())
        {
            QRegularExpression pattern(QRegularExpression::wildcardToRegularExpression(regExp), bCaseSensitive ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption);
            patIt = s_patternMap.insert(regExp, pattern);
        }

        if(patIt.value().match(testString).hasMatch())
            return true;
    }

    return false;
}

bool Utils::isCTokenChar(QChar c)
{
    //Locale aware but defaults to 'C' locale which is what we need.
    //QChar::toLatin1() returns 0 if not a Latin-1 character.
    return (c.toLatin1() == '_') || std::isalnum(c.toLatin1());
}

/// Calculate where a token starts and ends, given the x-position on screen.
void Utils::calcTokenPos(const QString& s, qint32 posOnScreen, qsizetype& pos1, qsizetype& pos2)
{
    qsizetype pos = std::max(0, posOnScreen);
    if(pos >= s.length())
    {
        pos1 = s.length();
        pos2 = s.length();
        return;
    }

    pos1 = pos;
    pos2 = pos + 1;

    if(isCTokenChar(s[pos1]))
    {
        while(pos1 >= 0 && isCTokenChar(s[pos1]))
            --pos1;
        ++pos1;

        while(pos2 < s.length() && isCTokenChar(s[pos2]))
            ++pos2;
    }
}

QString Utils::calcHistoryLead(const QString& s)
{
    static const QRegularExpression nonWhitespace("\\S"), whitespace("\\s");

    // Return the start of the line until the first white char after the first non white char.
    qsizetype i = s.indexOf(nonWhitespace);
    if(i == -1)
        return QString("");

    i = s.indexOf(whitespace, i);
    if(Q_UNLIKELY(i == -1))
        return s;// Very unlikely

    return s.left(i);
}

/*
    QUrl::toLocalFile does some special handling for locally visable windows network drives.
    If QUrl::isLocal however it returns false and we get an empty string back.
*/
QString Utils::urlToString(const QUrl &url)
{
    if(!FileAccess::isLocal(url))
        return url.toString();

    QString result = url.toLocalFile();
    if(result.isEmpty())
        return url.path();

    return result;
}

/*
    QStringConverter::availableCodecs() returns codecs that are from our perspective duplicates or
    are specialized single purpose codecs. Additionally Qt un-helpfully adds "Locale" to the list.
*/
QStringList Utils::availableCodecs(){
    const std::set<QString> ignored = { "Adobe-Standard-Encoding", "Extended_UNIX_Code_Packed_Format_for_Japanese","UTF-7" };
    const QRegularExpression regExp(",");
    qint32 codecCount = ucnv_countAvailable();
    QStringList result;

    regExp.optimize();
    result.reserve(codecCount);
    for(qint32 i = 0; i < codecCount; ++i)
    {
        UErrorCode status = U_ZERO_ERROR;
        const char* icuName = ucnv_getAvailableName(i);
        const char* standardName = ucnv_getStandardName(icuName, "IANA", &status);

        if(U_FAILURE(status) || !standardName) {
            continue;
        }

        const QString name = QString::fromLatin1(standardName);
        if(ignored.find(name) != ignored.cend())
            continue;

        result.push_back(name);
    }
    return result;
}
