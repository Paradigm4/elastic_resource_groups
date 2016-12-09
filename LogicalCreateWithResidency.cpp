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

#include "log4cxx/logger.h"
#include <query/Operator.h>
#include <set>
#include <usr_namespace/Permissions.h>
#include <NamespacesCommunicatorCopy.h>

#define fail(e) throw USER_EXCEPTION(SCIDB_SE_INFER_SCHEMA,e)

using namespace std;

namespace scidb
{

class LogicalCreateWithResidency : public LogicalOperator
{
public:
    LogicalCreateWithResidency(const string& logicalName,const string& alias)
     : LogicalOperator(logicalName,alias)
    {
        _properties.ddl       = true;
        ADD_PARAM_OUT_ARRAY_NAME()                       // The array name
        ADD_PARAM_SCHEMA()                               // The array schema
        ADD_PARAM_CONSTANT(TID_BOOL);                    // The temporary flag
        ADD_PARAM_CONSTANT(TID_UINT64);
        ADD_PARAM_VARIES();
    }

    vector<std::shared_ptr<OperatorParamPlaceholder> > nextVaryParamPlaceholder(const vector< ArrayDesc> &schemas)
    {
        vector<std::shared_ptr<OperatorParamPlaceholder> > res;
        res.push_back(PARAM_CONSTANT("uint64"));
        res.push_back(END_OF_VARIES_PARAMS());
        return res;
    }

    template<class t> shared_ptr<t>& param(size_t i) const
    {
        SCIDB_ASSERT(i < _parameters.size());
        return (std::shared_ptr<t>&)_parameters[i];
    }

    string inferPermissions(std::shared_ptr<Query>& query)
    {
        // Ensure we have permissions to create the array in the namespace
        std::string permissions;
        permissions.push_back(scidb::permissions::namespaces::CreateArray);
        return permissions;
    }

    ArrayDesc inferSchema(vector<ArrayDesc>,std::shared_ptr<Query> query)
    {
        assert(param<OperatorParam>(0)->getParamType() == PARAM_ARRAY_REF);
        assert(param<OperatorParam>(1)->getParamType() == PARAM_SCHEMA);

        string arrayNameOrg(param<OperatorParamArrayReference>(0)->getObjectName());

        std::string arrayName;
        std::string namespaceName;
        query->getNamespaceArrayNames(arrayNameOrg, namespaceName, arrayName);

        try
        {
            scidb::namespaces::Communicator::checkArrayAccess(namespaceName, arrayName);
        } catch(scidb::SystemException& e) {
            if(e.getLongErrorCode() != SCIDB_LE_ARRAY_DOESNT_EXIST)
            {
                throw;
            }
        }

        set<InstanceID> instances;
        for (size_t i = 3, n =_parameters.size(); i<n; ++i)
        {
            InstanceID instance = evaluate(((std::shared_ptr<OperatorParamLogicalExpression>&)_parameters[i])->getExpression(), query, TID_UINT64).getUint64();
            if (instances.count(instance) != 0)
            {
                ostringstream out;
                out<<"Instance ID "<<instance<<" specified multiple times.";
                throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << out.str();
            }
            if (query->isPhysicalInstanceDead(instance))
            {
                ostringstream out;
                out<<"Physical Instance ID "<<instance<<" is not currently alive.";
                throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << out.str();
            }
            instances.insert(instance);
        }
        ArrayResPtr resPtr(new MapArrayResidency(instances.begin(), instances.end()));
        ArrayDesc arrDesc;
        arrDesc.setDistribution(defaultPartitioning());
        arrDesc.setResidency(resPtr);
        return arrDesc;
    }

    void inferArrayAccess(std::shared_ptr<Query>& query)
    {
        LogicalOperator::inferArrayAccess(query);
        SCIDB_ASSERT(param<OperatorParam>(0)->getParamType() == PARAM_ARRAY_REF);
        string const& objName = param<OperatorParamArrayReference>(0)->getObjectName();
        SCIDB_ASSERT(!objName.empty());
        SCIDB_ASSERT(!ArrayDesc::isNameVersioned(objName)); // no version number
        std::string arrayName;
        std::string namespaceName;
        query->getNamespaceArrayNames(objName, namespaceName, arrayName);
        std::shared_ptr<SystemCatalog::LockDesc> lock(
            make_shared<SystemCatalog::LockDesc>(
                namespaceName,
                arrayName,
                query->getQueryID(),
                Cluster::getInstance()->getLocalInstanceId(),
                SystemCatalog::LockDesc::COORD,
                SystemCatalog::LockDesc::XCL));
        std::shared_ptr<SystemCatalog::LockDesc> resLock = query->requestLock(lock);
        SCIDB_ASSERT(resLock);
        SCIDB_ASSERT(resLock->getLockMode() >= SystemCatalog::LockDesc::XCL);
    }
};

REGISTER_LOGICAL_OPERATOR_FACTORY(LogicalCreateWithResidency, "create_with_residency");

} //namespace
