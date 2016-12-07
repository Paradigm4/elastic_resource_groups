/*
**
* BEGIN_COPYRIGHT
*
* Copyright (C) 2008-2016 SciDB, Inc.
* All Rights Reserved.
*
* variable_residency is a plugin for SciDB, an Open Source Array DBMS maintained
* by Paradigm4. See http://www.paradigm4.com/
*
* variable_residency is free software: you can redistribute it and/or modify
* it under the terms of the AFFERO GNU General Public License as published by
* the Free Software Foundation.
*
* variable_residency is distributed "AS-IS" AND WITHOUT ANY WARRANTY OF ANY KIND,
* INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,
* NON-INFRINGEMENT, OR FITNESS FOR A PARTICULAR PURPOSE. See
* the AFFERO GNU General Public License for the complete license terms.
*
* You should have received a copy of the AFFERO GNU General Public License
* along with variable_residency.  If not, see <http://www.gnu.org/licenses/agpl-3.0.html>
*
* END_COPYRIGHT
*/

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <query/Operator.h>
#include <log4cxx/logger.h>

using std::shared_ptr;

using namespace std;

namespace scidb
{

class LogicalStoreToInstances: public LogicalOperator
{
public:
    LogicalStoreToInstances(const std::string& logicalName, const std::string& alias) :
            LogicalOperator(logicalName, alias)
    {
        ADD_PARAM_INPUT();
    }

    ArrayDesc inferSchema(std::vector<ArrayDesc> schemas, shared_ptr<Query> query)
    {
        ArrayDesc const& inputSchema = schemas[0];
        return inputSchema;
    }
};

REGISTER_LOGICAL_OPERATOR_FACTORY(LogicalStoreToInstances, "store_to_instances");

} // emd namespace scidb
