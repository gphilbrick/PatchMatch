#ifndef IEC_QDATASTREAMOPERATORS_H
#define IEC_QDATASTREAMOPERATORS_H

#include <QDataStream>

#include <vector>
#include <map>

namespace core {
template<typename T>
class TwoDArray;
} // core

namespace coreQt {

void writeToDataStream( QDataStream&, const core::TwoDArray< bool >& );
void readFromDataStream( QDataStream&, core::TwoDArray< bool >& );

} // coreQt

#endif // #include 
