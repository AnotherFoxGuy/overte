 //
//  Bookmarks.h
//  interface/src
//
//  Created by David Rowe on 13 Jan 2015.
//  Copyright 2015 High Fidelity, Inc.
//  Copyright 2024 Overte e.V.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_Bookmarks_h
#define hifi_Bookmarks_h

#include <QMap>
#include <QObject>
#include <QPointer>

class QAction;
class QMenu;
class Menu;
class MenuWrapper;

class Bookmarks: public QObject {
    Q_OBJECT

public:
    Bookmarks() : _isMenuSorted(false) {}

    virtual void setupMenus(Menu* menubar, MenuWrapper* menu) = 0;
    void insert(const QString& name, const QVariant& address);  // Overwrites any existing entry with same name.
    QString addressForBookmark(const QString& name) const;

    const QString& getBookmarkError() const { return _bookmarkError; }

protected:
    void deleteBookmark(const QString& bookmarkName);

    void addBookmarkToFile(const QString& bookmarkName, const QVariant& bookmark);
    virtual void addBookmarkToMenu(Menu* menubar, const QString& name, const QVariant& bookmark) = 0;
    void enableMenuItems(bool enabled);
    virtual void readFromFile();
    void sortActions(Menu* menubar, MenuWrapper* menu);
    int getMenuItemLocation(QList<QAction*> actions, const QString& name) const;
    void removeBookmarkFromMenu(Menu* menubar, const QString& name);
    bool contains(const QString& name) const;
    void remove(const QString& name);

    QVariantMap _bookmarks;  // { name: url, ... }
    QString _bookmarkError;
    QPointer<MenuWrapper> _bookmarksMenu;
    QPointer<QAction> _deleteBookmarksAction;
    QString _bookmarksFilename;
    bool _isMenuSorted;

protected slots:
    /*@jsdoc
     * Prompts the user to delete a bookmark. The user can select the bookmark to delete in the dialog that is opened.
     * @function LocationBookmarks.deleteBookmark
     */
    virtual void deleteBookmark();

private:
    static bool sortOrder(QAction* a, QAction* b);

    void persistToFile();
};

#endif // hifi_Bookmarks_h
