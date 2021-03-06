﻿#pragma once

#include <qobject.h>
#include <qstandarditemmodel.h>
#include <qabstractitemview.h>
#include <functional>

#include <qtableview.h>
#include <qheaderview.h>

template<typename T>
class AbstractModelHelper;

template<typename T>
class DisplayModelHelper;

template<typename T>
class ItemDataEditor : QObject {
public:
    ItemDataEditor() = delete;

    T& operator()() {
        return *data;
    }

    void apply() {
        callback();
        deleteLater();
    }

    friend class AbstractModelHelper<T>;

private:
    T* data;
    std::function<void()> callback;

    ItemDataEditor(T* data, std::function<void()> callback) {
        this->data = data;
        this->callback = callback;
    }
};

#define BIND_COL(member) [](auto& data) -> auto& { return data.member; }

template<typename T>
class ModelCaster {
public:
    template<typename E>
    ModelCaster& next(E&(*ptr)(T&), Qt::ItemDataRole role = Qt::DisplayRole, bool editable = false) {
        setters << [=](T& data, QStandardItem* item) {
            if (ptr != nullptr) {
                item->setData(QVariant::fromValue(ptr(data)), role);
            }
            item->setEditable(editable);
        };

        getters << [=](T& data, const QStandardItem* item) {
            if (ptr != nullptr) {
                ptr(data) = item->data(role).value<E>();
            }
        };

        return *this;
    }

    ModelCaster& next(bool editable = false) {
        setters << [=](T& data, QStandardItem* item) {
            item->setEditable(editable);
        };

        getters << [=](T& data, const QStandardItem* item) {};

        return *this;
    }

    friend class AbstractModelHelper<T>;

private:
    QList<std::function<void(T&, QStandardItem*)>> setters;
    QList<std::function<void(T&, const QStandardItem*)>> getters;
};

#define DISPLAY_COL(member) [](const auto& data) -> auto { return data.member; }

template<typename T>
class DisplayCaster {
public:
    template<typename E>
    DisplayCaster& next(E(*ptr)(const T&), Qt::ItemDataRole role = Qt::DisplayRole) {
        setters << [=](const T& data, QStandardItem* item) {
            if (ptr != nullptr) {
                item->setData(QVariant::fromValue(ptr(data)), role);
            }
        };
        return *this;
    }

    DisplayCaster& next() {
        setters << [=](const T& data, QStandardItem* item) {};
        return *this;
    }

    friend class DisplayModelHelper<T>;

private:
    QList<std::function<void(const T&, QStandardItem*)>> setters;
};

template<typename T>
class AbstractModelHelper : QObject {
public:
    AbstractModelHelper(QAbstractItemView* view)
        : QObject(view) 
        , model(new QStandardItemModel(this))
        , view(view)
    {
        attatchModel();
    }

    void attatchModel() {
        view->setModel(model);
    }

    void reset() {
        model->clear();
        data.clear();
        createHeader(getHeaders());
    }

    int append(T& d) {
        data.append(d);
        int colSize = model->columnCount();
        QList<QStandardItem*> items;
        for (int i = 0; i < colSize; i++) {
            auto item = new QStandardItem;
            modelCaster.setters.at(i)(d, item);
            items << item;
        }
        model->appendRow(items);
        return model->rowCount() - 1;
    }

    void prepend(T& d) {
        data.prepend(d);
        int colSize = model->columnCount();
        QList<QStandardItem*> items;
        for (int i = 0; i < colSize; i++) {
            auto item = new QStandardItem;
            modelCaster.setters.at(i)(d, item);
            items << item;
        }
        model->insertRow(0, items);
    }

    T takeRow(int index) {
        model->removeRow(index);
        return data.takeAt(index);
    }

    T takeLast() {
        return takeRow(model->rowCount() - 1);
    }

    T takeFirst() {
        return takeRow(0);
    }

    int getRowCount() {
        return model->rowCount();
    }

    const QList<T>& getData() {
        return data;
    }

    void update() {
        int rowSize = model->rowCount();
        int colSize = model->columnCount();
        for (int i = 0; i < rowSize; i++) {
            for (int j = 0; j < colSize; j++) {
                modelCaster.getters.at(j)(data[i], model->item(i, j));
            }
        }
    }

    void update(int row) {
        int colSize = model->columnCount();
        for (int j = 0; j < colSize; j++) {
            modelCaster.getters.at(j)(data[row], model->item(row, j));
        }
    }

    ItemDataEditor<T>& editRow(int rowIndex) {
        return *new ItemDataEditor<T>(&data[rowIndex], [=] {
            rowDataReseted(rowIndex);
        });
    }

    QStandardItemModel* getModel() {
        return model;
    }

    QAbstractItemView* getView() {
        return view;
    }

    template<typename V>
    V* getView() {
        return dynamic_cast<V*>(view);
    }

protected:
    void createHeader(const QStringList& headers) {
        if (!headers.isEmpty()) {
            model->setColumnCount(headers.size());
            int col = 0;
            for (const auto& h : headers) {
                model->setHeaderData(col++, Qt::Horizontal, h, Qt::DisplayRole);
            }
        } else {
            model->setColumnCount(1);
        }
        onHeaderCreated(model->columnCount());
    }

    virtual void rowDataReseted(int rowIndex) {
        int colSize = model->columnCount();
        for (int j = 0; j < colSize; j++) {
            modelCaster.setters.at(j)(data[rowIndex], model->item(rowIndex, j));
        }
    }

protected:
    virtual QStringList getHeaders() = 0;

    virtual void onHeaderCreated(int colSize) {}

    ModelCaster<T>& getModelCaster() {
        return modelCaster;
    }

    QHeaderView* tbHHeader() {
        return static_cast<QTableView*>(view)->horizontalHeader();
    }

    QHeaderView* tbVHeader() {
        return static_cast<QTableView*>(view)->verticalHeader();
    }

protected:
    QList<T> data;
    QStandardItemModel* model;
    QAbstractItemView* view;

    ModelCaster<T> modelCaster;
};

template<typename T>
class DisplayModelHelper: AbstractModelHelper<T> {
public:
    using AbstractModelHelper::AbstractModelHelper;
    using AbstractModelHelper::attatchModel;
    using AbstractModelHelper::reset;

    int append(const T& d) {
        data.append(d);
        int colSize = model->columnCount();
        QList<QStandardItem*> items;
        for (int i = 0; i < colSize; i++) {
            auto item = new QStandardItem;
            modelCaster.setters.at(i)(d, item);
            items << item;
        }
        model->appendRow(items);
        return model->rowCount() - 1;
    }

    void prepend(const T& d) {
        data.prepend(d);
        int colSize = model->columnCount();
        QList<QStandardItem*> items;
        for (int i = 0; i < colSize; i++) {
            auto item = new QStandardItem;
            modelCaster.setters.at(i)(d, item);
            items << item;
        }
        model->insertRow(0, items);
    }

    using AbstractModelHelper::takeRow;
    using AbstractModelHelper::takeFirst;
    using AbstractModelHelper::takeLast;

    using AbstractModelHelper::getRowCount;

    using AbstractModelHelper::getData;
    using AbstractModelHelper::editRow;

    using AbstractModelHelper::getModel;
    using AbstractModelHelper::getView;

protected:
    DisplayCaster<T>& getModelCaster() {
        return modelCaster;
    }

    using AbstractModelHelper::tbHHeader;
    using AbstractModelHelper::tbVHeader;

    void rowDataReseted(int rowIndex) override {
        int colSize = model->columnCount();
        for (int j = 0; j < colSize; j++) {
            modelCaster.setters.at(j)(data[rowIndex], model->item(rowIndex, j));
        }
    }

protected:
    DisplayCaster<T> modelCaster;
};