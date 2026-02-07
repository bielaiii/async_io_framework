#ifndef I_OPERATION_HEADER
#define I_OPERATION_HEADER
#include <memory>
struct operation  {
public:
    virtual ~operation()             = default;
    virtual void complete() noexcept = 0;
};


#endif 

