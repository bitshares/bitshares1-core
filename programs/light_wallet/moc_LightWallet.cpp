/****************************************************************************
** Meta object code from reading C++ file 'LightWallet.hpp'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.4.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "LightWallet.hpp"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'LightWallet.hpp' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.4.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
struct qt_meta_stringdata_LightWallet_t {
    QByteArrayData data[61];
    char stringdata[731];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_LightWallet_t, stringdata) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_LightWallet_t qt_meta_stringdata_LightWallet = {
    {
QT_MOC_LITERAL(0, 0, 11), // "LightWallet"
QT_MOC_LITERAL(1, 12, 19), // "walletExistsChanged"
QT_MOC_LITERAL(2, 32, 0), // ""
QT_MOC_LITERAL(3, 33, 6), // "exists"
QT_MOC_LITERAL(4, 40, 15), // "errorConnecting"
QT_MOC_LITERAL(5, 56, 5), // "error"
QT_MOC_LITERAL(6, 62, 12), // "errorOpening"
QT_MOC_LITERAL(7, 75, 14), // "errorUnlocking"
QT_MOC_LITERAL(8, 90, 16), // "errorRegistering"
QT_MOC_LITERAL(9, 107, 16), // "connectedChanged"
QT_MOC_LITERAL(10, 124, 9), // "connected"
QT_MOC_LITERAL(11, 134, 11), // "openChanged"
QT_MOC_LITERAL(12, 146, 4), // "open"
QT_MOC_LITERAL(13, 151, 15), // "unlockedChanged"
QT_MOC_LITERAL(14, 167, 8), // "unlocked"
QT_MOC_LITERAL(15, 176, 15), // "brainKeyChanged"
QT_MOC_LITERAL(16, 192, 3), // "key"
QT_MOC_LITERAL(17, 196, 6), // "synced"
QT_MOC_LITERAL(18, 203, 12), // "notification"
QT_MOC_LITERAL(19, 216, 7), // "message"
QT_MOC_LITERAL(20, 224, 15), // "accountsChanged"
QT_MOC_LITERAL(21, 240, 3), // "arg"
QT_MOC_LITERAL(22, 244, 16), // "allAssetsChanged"
QT_MOC_LITERAL(23, 261, 19), // "pollForRegistration"
QT_MOC_LITERAL(24, 281, 11), // "accountName"
QT_MOC_LITERAL(25, 293, 15), // "connectToServer"
QT_MOC_LITERAL(26, 309, 4), // "host"
QT_MOC_LITERAL(27, 314, 4), // "port"
QT_MOC_LITERAL(28, 319, 9), // "serverKey"
QT_MOC_LITERAL(29, 329, 4), // "user"
QT_MOC_LITERAL(30, 334, 8), // "password"
QT_MOC_LITERAL(31, 343, 20), // "disconnectFromServer"
QT_MOC_LITERAL(32, 364, 12), // "createWallet"
QT_MOC_LITERAL(33, 377, 13), // "recoverWallet"
QT_MOC_LITERAL(34, 391, 8), // "brainKey"
QT_MOC_LITERAL(35, 400, 10), // "openWallet"
QT_MOC_LITERAL(36, 411, 11), // "closeWallet"
QT_MOC_LITERAL(37, 423, 12), // "unlockWallet"
QT_MOC_LITERAL(38, 436, 10), // "lockWallet"
QT_MOC_LITERAL(39, 447, 15), // "registerAccount"
QT_MOC_LITERAL(40, 463, 13), // "clearBrainKey"
QT_MOC_LITERAL(41, 477, 4), // "sync"
QT_MOC_LITERAL(42, 482, 15), // "syncAllBalances"
QT_MOC_LITERAL(43, 498, 19), // "syncAllTransactions"
QT_MOC_LITERAL(44, 518, 6), // "getFee"
QT_MOC_LITERAL(45, 525, 8), // "Balance*"
QT_MOC_LITERAL(46, 534, 11), // "assetSymbol"
QT_MOC_LITERAL(47, 546, 20), // "getDigitsOfPrecision"
QT_MOC_LITERAL(48, 567, 13), // "accountExists"
QT_MOC_LITERAL(49, 581, 4), // "name"
QT_MOC_LITERAL(50, 586, 18), // "isValidAccountName"
QT_MOC_LITERAL(51, 605, 14), // "verifyBrainKey"
QT_MOC_LITERAL(52, 620, 12), // "walletExists"
QT_MOC_LITERAL(53, 633, 12), // "accountNames"
QT_MOC_LITERAL(54, 646, 8), // "accounts"
QT_MOC_LITERAL(55, 655, 9), // "allAssets"
QT_MOC_LITERAL(56, 665, 15), // "connectionError"
QT_MOC_LITERAL(57, 681, 9), // "openError"
QT_MOC_LITERAL(58, 691, 11), // "unlockError"
QT_MOC_LITERAL(59, 703, 11), // "maxMemoSize"
QT_MOC_LITERAL(60, 715, 15) // "baseAssetSymbol"

    },
    "LightWallet\0walletExistsChanged\0\0"
    "exists\0errorConnecting\0error\0errorOpening\0"
    "errorUnlocking\0errorRegistering\0"
    "connectedChanged\0connected\0openChanged\0"
    "open\0unlockedChanged\0unlocked\0"
    "brainKeyChanged\0key\0synced\0notification\0"
    "message\0accountsChanged\0arg\0"
    "allAssetsChanged\0pollForRegistration\0"
    "accountName\0connectToServer\0host\0port\0"
    "serverKey\0user\0password\0disconnectFromServer\0"
    "createWallet\0recoverWallet\0brainKey\0"
    "openWallet\0closeWallet\0unlockWallet\0"
    "lockWallet\0registerAccount\0clearBrainKey\0"
    "sync\0syncAllBalances\0syncAllTransactions\0"
    "getFee\0Balance*\0assetSymbol\0"
    "getDigitsOfPrecision\0accountExists\0"
    "name\0isValidAccountName\0verifyBrainKey\0"
    "walletExists\0accountNames\0accounts\0"
    "allAssets\0connectionError\0openError\0"
    "unlockError\0maxMemoSize\0baseAssetSymbol"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_LightWallet[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
      35,   14, // methods
      13,  300, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
      13,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,  189,    2, 0x06 /* Public */,
       4,    1,  192,    2, 0x06 /* Public */,
       6,    1,  195,    2, 0x06 /* Public */,
       7,    1,  198,    2, 0x06 /* Public */,
       8,    1,  201,    2, 0x06 /* Public */,
       9,    1,  204,    2, 0x06 /* Public */,
      11,    1,  207,    2, 0x06 /* Public */,
      13,    1,  210,    2, 0x06 /* Public */,
      15,    1,  213,    2, 0x06 /* Public */,
      17,    0,  216,    2, 0x06 /* Public */,
      18,    1,  217,    2, 0x06 /* Public */,
      20,    1,  220,    2, 0x06 /* Public */,
      22,    0,  223,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
      23,    1,  224,    2, 0x0a /* Public */,
      25,    5,  227,    2, 0x0a /* Public */,
      25,    4,  238,    2, 0x2a /* Public | MethodCloned */,
      25,    3,  247,    2, 0x2a /* Public | MethodCloned */,
      25,    2,  254,    2, 0x2a /* Public | MethodCloned */,
      31,    0,  259,    2, 0x0a /* Public */,
      32,    2,  260,    2, 0x0a /* Public */,
      33,    3,  265,    2, 0x0a /* Public */,
      35,    0,  272,    2, 0x0a /* Public */,
      36,    0,  273,    2, 0x0a /* Public */,
      37,    1,  274,    2, 0x0a /* Public */,
      38,    0,  277,    2, 0x0a /* Public */,
      39,    1,  278,    2, 0x0a /* Public */,
      40,    0,  281,    2, 0x0a /* Public */,
      41,    0,  282,    2, 0x0a /* Public */,
      42,    0,  283,    2, 0x0a /* Public */,
      43,    0,  284,    2, 0x0a /* Public */,

 // methods: name, argc, parameters, tag, flags
      44,    1,  285,    2, 0x02 /* Public */,
      47,    1,  288,    2, 0x02 /* Public */,
      48,    1,  291,    2, 0x02 /* Public */,
      50,    1,  294,    2, 0x02 /* Public */,
      51,    1,  297,    2, 0x02 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Bool,    3,
    QMetaType::Void, QMetaType::QString,    5,
    QMetaType::Void, QMetaType::QString,    5,
    QMetaType::Void, QMetaType::QString,    5,
    QMetaType::Void, QMetaType::QString,    5,
    QMetaType::Void, QMetaType::Bool,   10,
    QMetaType::Void, QMetaType::Bool,   12,
    QMetaType::Void, QMetaType::Bool,   14,
    QMetaType::Void, QMetaType::QString,   16,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   19,
    QMetaType::Void, QMetaType::QVariantMap,   21,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, QMetaType::QString,   24,
    QMetaType::Void, QMetaType::QString, QMetaType::UShort, QMetaType::QString, QMetaType::QString, QMetaType::QString,   26,   27,   28,   29,   30,
    QMetaType::Void, QMetaType::QString, QMetaType::UShort, QMetaType::QString, QMetaType::QString,   26,   27,   28,   29,
    QMetaType::Void, QMetaType::QString, QMetaType::UShort, QMetaType::QString,   26,   27,   28,
    QMetaType::Void, QMetaType::QString, QMetaType::UShort,   26,   27,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   24,   30,
    QMetaType::Bool, QMetaType::QString, QMetaType::QString, QMetaType::QString,   24,   30,   34,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   30,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   24,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

 // methods: parameters
    0x80000000 | 45, QMetaType::QString,   46,
    QMetaType::Int, QMetaType::QString,   46,
    QMetaType::Bool, QMetaType::QString,   49,
    QMetaType::Bool, QMetaType::QString,   49,
    QMetaType::Bool, QMetaType::QString,   16,

 // properties: name, type, flags
      52, QMetaType::Bool, 0x00485001,
      10, QMetaType::Bool, 0x00495001,
      12, QMetaType::Bool, 0x00495001,
      14, QMetaType::Bool, 0x00495001,
      53, QMetaType::QStringList, 0x00495001,
      54, QMetaType::QVariantMap, 0x00495001,
      55, QMetaType::QStringList, 0x00495001,
      56, QMetaType::QString, 0x00495001,
      57, QMetaType::QString, 0x00495001,
      58, QMetaType::QString, 0x00495001,
      34, QMetaType::QString, 0x00495001,
      59, QMetaType::Short, 0x00095401,
      60, QMetaType::QString, 0x00095401,

 // properties: notify_signal_id
       0,
       5,
       6,
       7,
      11,
      11,
      12,
       1,
       2,
       3,
       8,
       0,
       0,

       0        // eod
};

void LightWallet::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        LightWallet *_t = static_cast<LightWallet *>(_o);
        switch (_id) {
        case 0: _t->walletExistsChanged((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 1: _t->errorConnecting((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 2: _t->errorOpening((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 3: _t->errorUnlocking((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 4: _t->errorRegistering((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 5: _t->connectedChanged((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 6: _t->openChanged((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 7: _t->unlockedChanged((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 8: _t->brainKeyChanged((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 9: _t->synced(); break;
        case 10: _t->notification((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 11: _t->accountsChanged((*reinterpret_cast< QVariantMap(*)>(_a[1]))); break;
        case 12: _t->allAssetsChanged(); break;
        case 13: _t->pollForRegistration((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 14: _t->connectToServer((*reinterpret_cast< QString(*)>(_a[1])),(*reinterpret_cast< quint16(*)>(_a[2])),(*reinterpret_cast< QString(*)>(_a[3])),(*reinterpret_cast< QString(*)>(_a[4])),(*reinterpret_cast< QString(*)>(_a[5]))); break;
        case 15: _t->connectToServer((*reinterpret_cast< QString(*)>(_a[1])),(*reinterpret_cast< quint16(*)>(_a[2])),(*reinterpret_cast< QString(*)>(_a[3])),(*reinterpret_cast< QString(*)>(_a[4]))); break;
        case 16: _t->connectToServer((*reinterpret_cast< QString(*)>(_a[1])),(*reinterpret_cast< quint16(*)>(_a[2])),(*reinterpret_cast< QString(*)>(_a[3]))); break;
        case 17: _t->connectToServer((*reinterpret_cast< QString(*)>(_a[1])),(*reinterpret_cast< quint16(*)>(_a[2]))); break;
        case 18: _t->disconnectFromServer(); break;
        case 19: _t->createWallet((*reinterpret_cast< QString(*)>(_a[1])),(*reinterpret_cast< QString(*)>(_a[2]))); break;
        case 20: { bool _r = _t->recoverWallet((*reinterpret_cast< QString(*)>(_a[1])),(*reinterpret_cast< QString(*)>(_a[2])),(*reinterpret_cast< QString(*)>(_a[3])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 21: _t->openWallet(); break;
        case 22: _t->closeWallet(); break;
        case 23: _t->unlockWallet((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 24: _t->lockWallet(); break;
        case 25: _t->registerAccount((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 26: _t->clearBrainKey(); break;
        case 27: _t->sync(); break;
        case 28: _t->syncAllBalances(); break;
        case 29: _t->syncAllTransactions(); break;
        case 30: { Balance* _r = _t->getFee((*reinterpret_cast< QString(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< Balance**>(_a[0]) = _r; }  break;
        case 31: { int _r = _t->getDigitsOfPrecision((*reinterpret_cast< QString(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = _r; }  break;
        case 32: { bool _r = _t->accountExists((*reinterpret_cast< QString(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 33: { bool _r = _t->isValidAccountName((*reinterpret_cast< QString(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 34: { bool _r = _t->verifyBrainKey((*reinterpret_cast< QString(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        void **func = reinterpret_cast<void **>(_a[1]);
        {
            typedef void (LightWallet::*_t)(bool );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&LightWallet::walletExistsChanged)) {
                *result = 0;
            }
        }
        {
            typedef void (LightWallet::*_t)(QString );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&LightWallet::errorConnecting)) {
                *result = 1;
            }
        }
        {
            typedef void (LightWallet::*_t)(QString );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&LightWallet::errorOpening)) {
                *result = 2;
            }
        }
        {
            typedef void (LightWallet::*_t)(QString );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&LightWallet::errorUnlocking)) {
                *result = 3;
            }
        }
        {
            typedef void (LightWallet::*_t)(QString );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&LightWallet::errorRegistering)) {
                *result = 4;
            }
        }
        {
            typedef void (LightWallet::*_t)(bool );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&LightWallet::connectedChanged)) {
                *result = 5;
            }
        }
        {
            typedef void (LightWallet::*_t)(bool );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&LightWallet::openChanged)) {
                *result = 6;
            }
        }
        {
            typedef void (LightWallet::*_t)(bool );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&LightWallet::unlockedChanged)) {
                *result = 7;
            }
        }
        {
            typedef void (LightWallet::*_t)(QString );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&LightWallet::brainKeyChanged)) {
                *result = 8;
            }
        }
        {
            typedef void (LightWallet::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&LightWallet::synced)) {
                *result = 9;
            }
        }
        {
            typedef void (LightWallet::*_t)(QString );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&LightWallet::notification)) {
                *result = 10;
            }
        }
        {
            typedef void (LightWallet::*_t)(QVariantMap );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&LightWallet::accountsChanged)) {
                *result = 11;
            }
        }
        {
            typedef void (LightWallet::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&LightWallet::allAssetsChanged)) {
                *result = 12;
            }
        }
    }
}

const QMetaObject LightWallet::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_LightWallet.data,
      qt_meta_data_LightWallet,  qt_static_metacall, Q_NULLPTR, Q_NULLPTR}
};


const QMetaObject *LightWallet::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *LightWallet::qt_metacast(const char *_clname)
{
    if (!_clname) return Q_NULLPTR;
    if (!strcmp(_clname, qt_meta_stringdata_LightWallet.stringdata))
        return static_cast<void*>(const_cast< LightWallet*>(this));
    return QObject::qt_metacast(_clname);
}

int LightWallet::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 35)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 35;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 35)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 35;
    }
#ifndef QT_NO_PROPERTIES
      else if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< bool*>(_v) = walletExists(); break;
        case 1: *reinterpret_cast< bool*>(_v) = isConnected(); break;
        case 2: *reinterpret_cast< bool*>(_v) = isOpen(); break;
        case 3: *reinterpret_cast< bool*>(_v) = isUnlocked(); break;
        case 4: *reinterpret_cast< QStringList*>(_v) = accountNames(); break;
        case 5: *reinterpret_cast< QVariantMap*>(_v) = accounts(); break;
        case 6: *reinterpret_cast< QStringList*>(_v) = allAssets(); break;
        case 7: *reinterpret_cast< QString*>(_v) = connectionError(); break;
        case 8: *reinterpret_cast< QString*>(_v) = openError(); break;
        case 9: *reinterpret_cast< QString*>(_v) = unlockError(); break;
        case 10: *reinterpret_cast< QString*>(_v) = brainKey(); break;
        case 11: *reinterpret_cast< qint16*>(_v) = maxMemoSize(); break;
        case 12: *reinterpret_cast< QString*>(_v) = baseAssetSymbol(); break;
        default: break;
        }
        _id -= 13;
    } else if (_c == QMetaObject::WriteProperty) {
        _id -= 13;
    } else if (_c == QMetaObject::ResetProperty) {
        _id -= 13;
    } else if (_c == QMetaObject::QueryPropertyDesignable) {
        _id -= 13;
    } else if (_c == QMetaObject::QueryPropertyScriptable) {
        _id -= 13;
    } else if (_c == QMetaObject::QueryPropertyStored) {
        _id -= 13;
    } else if (_c == QMetaObject::QueryPropertyEditable) {
        _id -= 13;
    } else if (_c == QMetaObject::QueryPropertyUser) {
        _id -= 13;
    } else if (_c == QMetaObject::RegisterPropertyMetaType) {
        if (_id < 13)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 13;
    }
#endif // QT_NO_PROPERTIES
    return _id;
}

// SIGNAL 0
void LightWallet::walletExistsChanged(bool _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void LightWallet::errorConnecting(QString _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void LightWallet::errorOpening(QString _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void LightWallet::errorUnlocking(QString _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void LightWallet::errorRegistering(QString _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void LightWallet::connectedChanged(bool _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void LightWallet::openChanged(bool _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}

// SIGNAL 7
void LightWallet::unlockedChanged(bool _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}

// SIGNAL 8
void LightWallet::brainKeyChanged(QString _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 8, _a);
}

// SIGNAL 9
void LightWallet::synced()
{
    QMetaObject::activate(this, &staticMetaObject, 9, Q_NULLPTR);
}

// SIGNAL 10
void LightWallet::notification(QString _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 10, _a);
}

// SIGNAL 11
void LightWallet::accountsChanged(QVariantMap _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 11, _a);
}

// SIGNAL 12
void LightWallet::allAssetsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 12, Q_NULLPTR);
}
QT_END_MOC_NAMESPACE
