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

#include <limits>
#include <sstream>
#include <memory>
#include <string>
#include <vector>
#include <ctype.h>
#include <query/Operator.h>

using std::shared_ptr;
using std::make_shared;
using namespace std;

namespace scidb
{

using namespace scidb;

class PhysicalStoreToInstances : public PhysicalOperator
{
public:
	PhysicalStoreToInstances(std::string const& logicalName,
        std::string const& physicalName,
        Parameters const& parameters,
        ArrayDesc const& schema):
            PhysicalOperator(logicalName, physicalName, parameters, schema)
    {}

    std::shared_ptr< Array> execute(std::vector< std::shared_ptr< Array> >& inputArrays, std::shared_ptr<Query> query)
    {
        shared_ptr<Array>& inputArray = inputArrays[0];
        return inputArray;
    }
};

REGISTER_PHYSICAL_OPERATOR_FACTORY(PhysicalStoreToInstances, "store_to_instances", "PhysicalStoreToInstances");


} // end namespace scidb
