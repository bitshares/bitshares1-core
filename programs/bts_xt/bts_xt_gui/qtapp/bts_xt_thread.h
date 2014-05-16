#ifndef BTSXTTHREAD_H
#define BTSXTTHREAD_H

#include <QThread>

namespace boost { namespace program_options { class variables_map; } }
namespace bts { namespace rpc { class rpc_server; } }
namespace fc { class path; }

class BtsXtThread : public QThread
{
    Q_OBJECT
    const boost::program_options::variables_map& _option_variables;
    bts::rpc::rpc_server* _p_rpc_server;
    const fc::path& _datadir;
    bool _cancel;
    
public:
    BtsXtThread(const boost::program_options::variables_map& option_variables, const fc::path& datadir)
    : QThread(0), _option_variables(option_variables), _datadir(datadir), _cancel(false)
    {
    }
    
    void run() Q_DECL_OVERRIDE ;
    void cancel(){ _cancel = true; }

signals:
    void resultReady(const QString &s);
};


#endif