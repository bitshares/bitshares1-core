/****************************************************************************
** Meta object code from reading C++ file 'QtWrappers.hpp'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.4.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "QtWrappers.hpp"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'QtWrappers.hpp' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.4.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
struct qt_meta_stringdata_LedgerEntry_t {
    QByteArrayData data[8];
    char stringdata[53];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_LedgerEntry_t, stringdata) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_LedgerEntry_t qt_meta_stringdata_LedgerEntry = {
    {
QT_MOC_LITERAL(0, 0, 11), // "LedgerEntry"
QT_MOC_LITERAL(1, 12, 4), // "stub"
QT_MOC_LITERAL(2, 17, 0), // ""
QT_MOC_LITERAL(3, 18, 6), // "sender"
QT_MOC_LITERAL(4, 25, 8), // "receiver"
QT_MOC_LITERAL(5, 34, 6), // "amount"
QT_MOC_LITERAL(6, 41, 6), // "symbol"
QT_MOC_LITERAL(7, 48, 4) // "memo"

    },
    "LedgerEntry\0stub\0\0sender\0receiver\0"
    "amount\0symbol\0memo"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_LedgerEntry[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       1,   14, // methods
       5,   20, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   19,    2, 0x06 /* Public */,

 // signals: parameters
    QMetaType::Void,

 // properties: name, type, flags
       3, QMetaType::QString, 0x00495003,
       4, QMetaType::QString, 0x00495003,
       5, QMetaType::QReal, 0x00495003,
       6, QMetaType::QString, 0x00495003,
       7, QMetaType::QString, 0x00495003,

 // properties: notify_signal_id
       0,
       0,
       0,
       0,
       0,

       0        // eod
};

void LedgerEntry::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        LedgerEntry *_t = static_cast<LedgerEntry *>(_o);
        switch (_id) {
        case 0: _t->stub(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        void **func = reinterpret_cast<void **>(_a[1]);
        {
            typedef void (LedgerEntry::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&LedgerEntry::stub)) {
                *result = 0;
            }
        }
    }
    Q_UNUSED(_a);
}

const QMetaObject LedgerEntry::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_LedgerEntry.data,
      qt_meta_data_LedgerEntry,  qt_static_metacall, Q_NULLPTR, Q_NULLPTR}
};


const QMetaObject *LedgerEntry::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *LedgerEntry::qt_metacast(const char *_clname)
{
    if (!_clname) return Q_NULLPTR;
    if (!strcmp(_clname, qt_meta_stringdata_LedgerEntry.stringdata))
        return static_cast<void*>(const_cast< LedgerEntry*>(this));
    return QObject::qt_metacast(_clname);
}

int LedgerEntry::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 1)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 1)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 1;
    }
#ifndef QT_NO_PROPERTIES
      else if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = m_sender; break;
        case 1: *reinterpret_cast< QString*>(_v) = m_receiver; break;
        case 2: *reinterpret_cast< qreal*>(_v) = m_amount; break;
        case 3: *reinterpret_cast< QString*>(_v) = m_symbol; break;
        case 4: *reinterpret_cast< QString*>(_v) = m_memo; break;
        default: break;
        }
        _id -= 5;
    } else if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0:
            if (m_sender != *reinterpret_cast< QString*>(_v)) {
                m_sender = *reinterpret_cast< QString*>(_v);
                Q_EMIT stub();
            }
            break;
        case 1:
            if (m_receiver != *reinterpret_cast< QString*>(_v)) {
                m_receiver = *reinterpret_cast< QString*>(_v);
                Q_EMIT stub();
            }
            break;
        case 2:
            if (m_amount != *reinterpret_cast< qreal*>(_v)) {
                m_amount = *reinterpret_cast< qreal*>(_v);
                Q_EMIT stub();
            }
            break;
        case 3:
            if (m_symbol != *reinterpret_cast< QString*>(_v)) {
                m_symbol = *reinterpret_cast< QString*>(_v);
                Q_EMIT stub();
            }
            break;
        case 4:
            if (m_memo != *reinterpret_cast< QString*>(_v)) {
                m_memo = *reinterpret_cast< QString*>(_v);
                Q_EMIT stub();
            }
            break;
        default: break;
        }
        _id -= 5;
    } else if (_c == QMetaObject::ResetProperty) {
        _id -= 5;
    } else if (_c == QMetaObject::QueryPropertyDesignable) {
        _id -= 5;
    } else if (_c == QMetaObject::QueryPropertyScriptable) {
        _id -= 5;
    } else if (_c == QMetaObject::QueryPropertyStored) {
        _id -= 5;
    } else if (_c == QMetaObject::QueryPropertyEditable) {
        _id -= 5;
    } else if (_c == QMetaObject::QueryPropertyUser) {
        _id -= 5;
    } else if (_c == QMetaObject::RegisterPropertyMetaType) {
        if (_id < 5)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 5;
    }
#endif // QT_NO_PROPERTIES
    return _id;
}

// SIGNAL 0
void LedgerEntry::stub()
{
    QMetaObject::activate(this, &staticMetaObject, 0, Q_NULLPTR);
}
struct qt_meta_stringdata_TransactionSummary_t {
    QByteArrayData data[9];
    char stringdata[95];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_TransactionSummary_t, stringdata) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_TransactionSummary_t qt_meta_stringdata_TransactionSummary = {
    {
QT_MOC_LITERAL(0, 0, 18), // "TransactionSummary"
QT_MOC_LITERAL(1, 19, 4), // "stub"
QT_MOC_LITERAL(2, 24, 0), // ""
QT_MOC_LITERAL(3, 25, 2), // "id"
QT_MOC_LITERAL(4, 28, 9), // "timestamp"
QT_MOC_LITERAL(5, 38, 9), // "feeAmount"
QT_MOC_LITERAL(6, 48, 9), // "feeSymbol"
QT_MOC_LITERAL(7, 58, 6), // "ledger"
QT_MOC_LITERAL(8, 65, 29) // "QQmlListProperty<LedgerEntry>"

    },
    "TransactionSummary\0stub\0\0id\0timestamp\0"
    "feeAmount\0feeSymbol\0ledger\0"
    "QQmlListProperty<LedgerEntry>"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_TransactionSummary[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       1,   14, // methods
       5,   20, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   19,    2, 0x06 /* Public */,

 // signals: parameters
    QMetaType::Void,

 // properties: name, type, flags
       3, QMetaType::QString, 0x00495003,
       4, QMetaType::QString, 0x00495003,
       5, QMetaType::QString, 0x00495003,
       6, QMetaType::QString, 0x00495003,
       7, 0x80000000 | 8, 0x00095409,

 // properties: notify_signal_id
       0,
       0,
       0,
       0,
       0,

       0        // eod
};

void TransactionSummary::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        TransactionSummary *_t = static_cast<TransactionSummary *>(_o);
        switch (_id) {
        case 0: _t->stub(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        void **func = reinterpret_cast<void **>(_a[1]);
        {
            typedef void (TransactionSummary::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&TransactionSummary::stub)) {
                *result = 0;
            }
        }
    }
    Q_UNUSED(_a);
}

const QMetaObject TransactionSummary::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_TransactionSummary.data,
      qt_meta_data_TransactionSummary,  qt_static_metacall, Q_NULLPTR, Q_NULLPTR}
};


const QMetaObject *TransactionSummary::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *TransactionSummary::qt_metacast(const char *_clname)
{
    if (!_clname) return Q_NULLPTR;
    if (!strcmp(_clname, qt_meta_stringdata_TransactionSummary.stringdata))
        return static_cast<void*>(const_cast< TransactionSummary*>(this));
    return QObject::qt_metacast(_clname);
}

int TransactionSummary::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 1)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 1)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 1;
    }
#ifndef QT_NO_PROPERTIES
      else if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = m_id; break;
        case 1: *reinterpret_cast< QString*>(_v) = m_when; break;
        case 2: *reinterpret_cast< QString*>(_v) = m_feeAmount; break;
        case 3: *reinterpret_cast< QString*>(_v) = m_feeSymbol; break;
        case 4: *reinterpret_cast< QQmlListProperty<LedgerEntry>*>(_v) = ledger(); break;
        default: break;
        }
        _id -= 5;
    } else if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0:
            if (m_id != *reinterpret_cast< QString*>(_v)) {
                m_id = *reinterpret_cast< QString*>(_v);
                Q_EMIT stub();
            }
            break;
        case 1:
            if (m_when != *reinterpret_cast< QString*>(_v)) {
                m_when = *reinterpret_cast< QString*>(_v);
                Q_EMIT stub();
            }
            break;
        case 2:
            if (m_feeAmount != *reinterpret_cast< QString*>(_v)) {
                m_feeAmount = *reinterpret_cast< QString*>(_v);
                Q_EMIT stub();
            }
            break;
        case 3:
            if (m_feeSymbol != *reinterpret_cast< QString*>(_v)) {
                m_feeSymbol = *reinterpret_cast< QString*>(_v);
                Q_EMIT stub();
            }
            break;
        default: break;
        }
        _id -= 5;
    } else if (_c == QMetaObject::ResetProperty) {
        _id -= 5;
    } else if (_c == QMetaObject::QueryPropertyDesignable) {
        _id -= 5;
    } else if (_c == QMetaObject::QueryPropertyScriptable) {
        _id -= 5;
    } else if (_c == QMetaObject::QueryPropertyStored) {
        _id -= 5;
    } else if (_c == QMetaObject::QueryPropertyEditable) {
        _id -= 5;
    } else if (_c == QMetaObject::QueryPropertyUser) {
        _id -= 5;
    } else if (_c == QMetaObject::RegisterPropertyMetaType) {
        if (_id < 5)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 5;
    }
#endif // QT_NO_PROPERTIES
    return _id;
}

// SIGNAL 0
void TransactionSummary::stub()
{
    QMetaObject::activate(this, &staticMetaObject, 0, Q_NULLPTR);
}
struct qt_meta_stringdata_Balance_t {
    QByteArrayData data[10];
    char stringdata[80];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_Balance_t, stringdata) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_Balance_t qt_meta_stringdata_Balance = {
    {
QT_MOC_LITERAL(0, 0, 7), // "Balance"
QT_MOC_LITERAL(1, 8, 13), // "symbolChanged"
QT_MOC_LITERAL(2, 22, 0), // ""
QT_MOC_LITERAL(3, 23, 3), // "arg"
QT_MOC_LITERAL(4, 27, 13), // "amountChanged"
QT_MOC_LITERAL(5, 41, 12), // "yieldChanged"
QT_MOC_LITERAL(6, 54, 6), // "symbol"
QT_MOC_LITERAL(7, 61, 6), // "amount"
QT_MOC_LITERAL(8, 68, 5), // "yield"
QT_MOC_LITERAL(9, 74, 5) // "total"

    },
    "Balance\0symbolChanged\0\0arg\0amountChanged\0"
    "yieldChanged\0symbol\0amount\0yield\0total"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_Balance[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       3,   14, // methods
       4,   38, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   29,    2, 0x06 /* Public */,
       4,    1,   32,    2, 0x06 /* Public */,
       5,    1,   35,    2, 0x06 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::QReal,    3,
    QMetaType::Void, QMetaType::QReal,    3,
    QMetaType::Void, QMetaType::QReal,    3,

 // properties: name, type, flags
       6, QMetaType::QString, 0x00495003,
       7, QMetaType::QReal, 0x00495003,
       8, QMetaType::QReal, 0x00495003,
       9, QMetaType::QReal, 0x00485001,

 // properties: notify_signal_id
       0,
       1,
       2,
       1,

       0        // eod
};

void Balance::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Balance *_t = static_cast<Balance *>(_o);
        switch (_id) {
        case 0: _t->symbolChanged((*reinterpret_cast< qreal(*)>(_a[1]))); break;
        case 1: _t->amountChanged((*reinterpret_cast< qreal(*)>(_a[1]))); break;
        case 2: _t->yieldChanged((*reinterpret_cast< qreal(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        void **func = reinterpret_cast<void **>(_a[1]);
        {
            typedef void (Balance::*_t)(qreal );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&Balance::symbolChanged)) {
                *result = 0;
            }
        }
        {
            typedef void (Balance::*_t)(qreal );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&Balance::amountChanged)) {
                *result = 1;
            }
        }
        {
            typedef void (Balance::*_t)(qreal );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&Balance::yieldChanged)) {
                *result = 2;
            }
        }
    }
}

const QMetaObject Balance::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_Balance.data,
      qt_meta_data_Balance,  qt_static_metacall, Q_NULLPTR, Q_NULLPTR}
};


const QMetaObject *Balance::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Balance::qt_metacast(const char *_clname)
{
    if (!_clname) return Q_NULLPTR;
    if (!strcmp(_clname, qt_meta_stringdata_Balance.stringdata))
        return static_cast<void*>(const_cast< Balance*>(this));
    return QObject::qt_metacast(_clname);
}

int Balance::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 3;
    }
#ifndef QT_NO_PROPERTIES
      else if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = m_symbol; break;
        case 1: *reinterpret_cast< qreal*>(_v) = m_amount; break;
        case 2: *reinterpret_cast< qreal*>(_v) = m_yield; break;
        case 3: *reinterpret_cast< qreal*>(_v) = total(); break;
        default: break;
        }
        _id -= 4;
    } else if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0:
            if (m_symbol != *reinterpret_cast< QString*>(_v)) {
                m_symbol = *reinterpret_cast< QString*>(_v);
            }
            break;
        case 1:
            if (m_amount != *reinterpret_cast< qreal*>(_v)) {
                m_amount = *reinterpret_cast< qreal*>(_v);
                Q_EMIT amountChanged(m_amount);
            }
            break;
        case 2:
            if (m_yield != *reinterpret_cast< qreal*>(_v)) {
                m_yield = *reinterpret_cast< qreal*>(_v);
                Q_EMIT yieldChanged(m_yield);
            }
            break;
        default: break;
        }
        _id -= 4;
    } else if (_c == QMetaObject::ResetProperty) {
        _id -= 4;
    } else if (_c == QMetaObject::QueryPropertyDesignable) {
        _id -= 4;
    } else if (_c == QMetaObject::QueryPropertyScriptable) {
        _id -= 4;
    } else if (_c == QMetaObject::QueryPropertyStored) {
        _id -= 4;
    } else if (_c == QMetaObject::QueryPropertyEditable) {
        _id -= 4;
    } else if (_c == QMetaObject::QueryPropertyUser) {
        _id -= 4;
    } else if (_c == QMetaObject::RegisterPropertyMetaType) {
        if (_id < 4)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 4;
    }
#endif // QT_NO_PROPERTIES
    return _id;
}

// SIGNAL 0
void Balance::symbolChanged(qreal _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void Balance::amountChanged(qreal _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void Balance::yieldChanged(qreal _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}
QT_END_MOC_NAMESPACE
