// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2019-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on
#ifndef COMMENTPARSER_H
#define COMMENTPARSER_H

#include "TypeUtils.h"

#include <vector>

#include <QChar>
#include <QString>

class CommentParser
{
  public:
    virtual void processChar(const QString &line, const QChar &inChar) = 0;
    virtual void processLine(const QString &line) = 0;
    virtual void removeComment(QString &line) = 0;
    [[nodiscard]] virtual bool inComment() const = 0;
    [[nodiscard]] virtual bool isPureComment() const = 0;
    [[nodiscard]] virtual bool isSkipable() const = 0;
    virtual ~CommentParser() = default;
};

class DefaultCommentParser : public CommentParser
{
  private:
    typedef enum {none, singleLine, multiLine}CommentType;
  public:
    void processLine(const QString &line) override;
    [[nodiscard]] bool inComment() const override { return mCommentType != none; };
    [[nodiscard]] bool isPureComment() const override { return mIsPureComment; };
    [[nodiscard]] bool isSkipable() const override { return mIsCommentOrWhite; };

    void removeComment(QString &line) override;
  protected:
    friend class CommentParserTest;

    void processChar(const QString &line, const QChar &inChar) override;
    //For tests only.
    [[nodiscard]] bool isEscaped() const { return bIsEscaped; }
    [[nodiscard]] bool inString() const { return bInString; }

  private:
    QChar mLastChar, mStartChar;

    struct CommentRange
    {
        qsizetype startOffset = 0;
        qsizetype endOffset = 0;
    };

    qsizetype offset = -1;

    CommentRange lastComment;

    std::vector<CommentRange> comments;

    bool isFirstLine = false;
    bool mIsCommentOrWhite = false;
    bool mIsPureComment = false;
    bool bInString = false;
    bool bIsEscaped = false;

    CommentType mCommentType = none;
};

#endif // !COMMENTPASER_H
