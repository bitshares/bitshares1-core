/****************************************************************************
** Meta object code from reading C++ file 'Account.hpp'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.4.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "Account.hpp"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'Account.hpp' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.4.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
struct qt_meta_stringdata_Account_t {
    QByteArrayData data[35];
    char stringdata[428];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_Account_t, stringdata) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_Account_t qt_meta_stringdata_Account = {
    {
QT_MOC_LITERAL(0, 0, 7), // "Account"
QT_MOC_LITERAL(1, 8, 11), // "nameChanged"
QT_MOC_LITERAL(2, 20, 0), // ""
QT_MOC_LITERAL(3, 21, 3), // "arg"
QT_MOC_LITERAL(4, 25, 15), // "ownerKeyChanged"
QT_MOC_LITERAL(5, 41, 16), // "activeKeyChanged"
QT_MOC_LITERAL(6, 58, 19), // "isRegisteredChanged"
QT_MOC_LITERAL(7, 78, 23), // "registrationDateChanged"
QT_MOC_LITERAL(8, 102, 15), // "balancesChanged"
QT_MOC_LITERAL(9, 118, 5), // "error"
QT_MOC_LITERAL(10, 124, 11), // "errorString"
QT_MOC_LITERAL(11, 136, 7), // "setName"
QT_MOC_LITERAL(12, 144, 11), // "setOwnerKey"
QT_MOC_LITERAL(13, 156, 12), // "setActiveKey"
QT_MOC_LITERAL(14, 169, 7), // "balance"
QT_MOC_LITERAL(15, 177, 8), // "Balance*"
QT_MOC_LITERAL(16, 186, 6), // "symbol"
QT_MOC_LITERAL(17, 193, 15), // "availableAssets"
QT_MOC_LITERAL(18, 209, 18), // "transactionHistory"
QT_MOC_LITERAL(19, 228, 15), // "QList<QObject*>"
QT_MOC_LITERAL(20, 244, 12), // "asset_symbol"
QT_MOC_LITERAL(21, 257, 13), // "beginTransfer"
QT_MOC_LITERAL(22, 271, 19), // "TransactionSummary*"
QT_MOC_LITERAL(23, 291, 9), // "recipient"
QT_MOC_LITERAL(24, 301, 6), // "amount"
QT_MOC_LITERAL(25, 308, 4), // "memo"
QT_MOC_LITERAL(26, 313, 16), // "completeTransfer"
QT_MOC_LITERAL(27, 330, 8), // "password"
QT_MOC_LITERAL(28, 339, 4), // "name"
QT_MOC_LITERAL(29, 344, 8), // "ownerKey"
QT_MOC_LITERAL(30, 353, 9), // "activeKey"
QT_MOC_LITERAL(31, 363, 12), // "isRegistered"
QT_MOC_LITERAL(32, 376, 16), // "registrationDate"
QT_MOC_LITERAL(33, 393, 8), // "balances"
QT_MOC_LITERAL(34, 402, 25) // "QQmlListProperty<Balance>"

    },
    "Account\0nameChanged\0\0arg\0ownerKeyChanged\0"
    "activeKeyChanged\0isRegisteredChanged\0"
    "registrationDateChanged\0balancesChanged\0"
    "error\0errorString\0setName\0setOwnerKey\0"
    "setActiveKey\0balance\0Balance*\0symbol\0"
    "availableAssets\0transactionHistory\0"
    "QList<QObject*>\0asset_symbol\0beginTransfer\0"
    "TransactionSummary*\0recipient\0amount\0"
    "memo\0completeTransfer\0password\0name\0"
    "ownerKey\0activeKey\0isRegistered\0"
    "registrationDate\0balances\0"
    "QQmlListProperty<Balance>"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_Account[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
      15,   14, // methods
       7,  136, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       7,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   89,    2, 0x06 /* Public */,
       4,    1,   92,    2, 0x06 /* Public */,
       5,    1,   95,    2, 0x06 /* Public */,
       6,    1,   98,    2, 0x06 /* Public */,
       7,    1,  101,    2, 0x06 /* Public */,
       8,    0,  104,    2, 0x06 /* Public */,
       9,    1,  105,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
      11,    1,  108,    2, 0x0a /* Public */,
      12,    1,  111,    2, 0x0a /* Public */,
      13,    1,  114,    2, 0x0a /* Public */,

 // methods: name, argc, parameters, tag, flags
      14,    1,  117,    2, 0x02 /* Public */,
      17,    0,  120,    2, 0x02 /* Public */,
      18,    1,  121,    2, 0x02 /* Public */,
      21,    4,  124,    2, 0x02 /* Public */,
      26,    1,  133,    2, 0x02 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void, QMetaType::Bool,    3,
    QMetaType::Void, QMetaType::QDateTime,    3,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   10,

 // slots: parameters
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void, QMetaType::QString,    3,

 // methods: parameters
    0x80000000 | 15, QMetaType::QString,   16,
    QMetaType::QStringList,
    0x80000000 | 19, QMetaType::QString,   20,
    0x80000000 | 22, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,   23,   24,   16,   25,
    QMetaType::Bool, QMetaType::QString,   27,

 // properties: name, type, flags
      28, QMetaType::QString, 0x00495103,
      29, QMetaType::QString, 0x00495103,
      30, QMetaType::QString, 0x00495103,
      31, QMetaType::Bool, 0x00495001,
      32, QMetaType::QDateTime, 0x00495001,
      33, 0x80000000 | 34, 0x00495009,
      17, QMetaType::QStringList, 0x00485001,

 // properties: notify_signal_id
       0,
       1,
       2,
       3,
       4,
       5,
       5,

       0        // eod
};

void Account::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Account *_t = static_cast<Account *>(_o);
        switch (_id) {
        case 0: _t->nameChanged((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 1: _t->ownerKeyChanged((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 2: _t->activeKeyChanged((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 3: _t->isRegisteredChanged((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 4: _t->registrationDateChanged((*reinterpret_cast< QDateTime(*)>(_a[1]))); break;
        case 5: _t->balancesChanged(); break;
        case 6: _t->error((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 7: _t->setName((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 8: _t->setOwnerKey((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 9: _t->setActiveKey((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 10: { Balance* _r = _t->balance((*reinterpret_cast< QString(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< Balance**>(_a[0]) = _r; }  break;
        case 11: { QStringList _r = _t->availableAssets();
            if (_a[0]) *reinterpret_cast< QStringList*>(_a[0]) = _r; }  break;
        case 12: { QList<QObject*> _r = _t->transactionHistory((*reinterpret_cast< QString(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< QList<QObject*>*>(_a[0]) = _r; }  break;
        case 13: { TransactionSummary* _r = _t->beginTransfer((*reinterpret_cast< QString(*)>(_a[1])),(*reinterpret_cast< QString(*)>(_a[2])),(*reinterpret_cast< QString(*)>(_a[3])),(*reinterpret_cast< QString(*)>(_a[4])));
            if (_a[0]) *reinterpret_cast< TransactionSummary**>(_a[0]) = _r; }  break;
        case 14: { bool _r = _t->completeTransfer((*reinterpret_cast< QString(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        void **func = reinterpret_cast<void **>(_a[1]);
        {
            typedef void (Account::*_t)(QString );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&Account::nameChanged)) {
                *result = 0;
            }
        }
        {
            typedef void (Account::*_t)(QString );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&Account::ownerKeyChanged)) {
                *result = 1;
            }
        }
        {
            typedef void (Account::*_t)(QString );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&Account::activeKeyChanged)) {
                *result = 2;
            }
        }
        {
            typedef void (Account::*_t)(bool );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&Account::isRegisteredChanged)) {
                *result = 3;
            }
        }
        {
            typedef void (Account::*_t)(QDateTime );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&Account::registrationDateChanged)) {
                *result = 4;
            }
        }
        {
            typedef void (Account::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&Account::balancesChanged)) {
                *result = 5;
            }
        }
        {
            typedef void (Account::*_t)(QString );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&Account::error)) {
                *result = 6;
            }
        }
    }
}

const QMetaObject Account::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_Account.data,
      qt_meta_data_Account,  qt_static_metacall, Q_NULLPTR, Q_NULLPTR}
};


const QMetaObject *Account::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Account::qt_metacast(const char *_clname)
{
    if (!_clname) return Q_NULLPTR;
    if (!strcmp(_clname, qt_meta_stringdata_Account.stringdata))
        return static_cast<void*>(const_cast< Account*>(this));
    return QObject::qt_metacast(_clname);
}

int Account::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 15)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 15;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 15)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 15;
    }
#ifndef QT_NO_PROPERTIES
      else if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = name(); break;
        case 1: *reinterpret_cast< QString*>(_v) = ownerKey(); break;
        case 2: *reinterpret_cast< QString*>(_v) = activeKey(); break;
        case 3: *reinterpret_cast< bool*>(_v) = isRegistered(); break;
        case 4: *reinterpret_cast< QDateTime*>(_v) = registrationDate(); break;
        case 5: *reinterpret_cast< QQmlListProperty<Balance>*>(_v) = balances(); break;
        case 6: *reinterpret_cast< QStringList*>(_v) = availableAssets(); break;
        default: break;
        }
        _id -= 7;
    } else if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: setName(*reinterpret_cast< QString*>(_v)); break;
        case 1: setOwnerKey(*reinterpret_cast< QString*>(_v)); break;
        case 2: setActiveKey(*reinterpret_cast< QString*>(_v)); break;
        default: break;
        }
        _id -= 7;
    } else if (_c == QMetaObject::ResetProperty) {
        _id -= 7;
    } else if (_c == QMetaObject::QueryPropertyDesignable) {
        _id -= 7;
    } else if (_c == QMetaObject::QueryPropertyScriptable) {
        _id -= 7;
    } else if (_c == QMetaObject::QueryPropertyStored) {
        _id -= 7;
    } else if (_c == QMetaObject::QueryPropertyEditable) {
        _id -= 7;
    } else if (_c == QMetaObject::QueryPropertyUser) {
        _id -= 7;
    } else if (_c == QMetaObject::RegisterPropertyMetaType) {
        if (_id < 7)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 7;
    }
#endif // QT_NO_PROPERTIES
    return _id;
}

// SIGNAL 0
void Account::nameChanged(QString _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void Account::ownerKeyChanged(QString _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void Account::activeKeyChanged(QString _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void Account::isRegisteredChanged(bool _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void Account::registrationDateChanged(QDateTime _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void Account::balancesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, Q_NULLPTR);
}

// SIGNAL 6
void Account::error(QString _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}
QT_END_MOC_NAMESPACE
