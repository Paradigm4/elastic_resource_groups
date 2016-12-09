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

#include <array/Metadata.h>
#include <boost/array.hpp>
#include <system/SystemCatalog.h>
#include <query/Operator.h>
#include <log4cxx/logger.h>
#include <array/TransientCache.h>
#include <util/session/Session.h>
#include <NamespacesCommunicatorCopy.h>

using namespace std;

namespace scidb
{
class PhysicalCreateWithResidency : public PhysicalOperator
{
public:
    PhysicalCreateWithResidency(const string& logicalName,
                                const string& physicalName,
                                const Parameters& parameters,
                                const ArrayDesc& schema)
     : PhysicalOperator(logicalName,physicalName,parameters,schema)
    {}

    template<class t>
    std::shared_ptr<t>& param(size_t i) const
    {
        assert(i < _parameters.size());
        return (std::shared_ptr<t>&)_parameters[i];
    }

    virtual std::shared_ptr<Array> execute(vector<shared_ptr<Array> >& in,shared_ptr<Query> query)
    {
        bool const temp( param<OperatorParamPhysicalExpression>(2)-> getExpression()->evaluate().getBool());
        if (query->isCoordinator())
        {
            string arrayNameOrg(param<OperatorParamArrayReference>(0)->getObjectName());
            std::string arrayName;
            std::string namespaceName;
            query->getNamespaceArrayNames(arrayNameOrg, namespaceName, arrayName);
            ArrayDesc arrSchema(param<OperatorParamSchema>(1)->getSchema());
            assert(ArrayDesc::isNameUnversioned(arrayName));
            arrSchema.setName(arrayName);
            arrSchema.setNamespaceName(namespaceName);
            arrSchema.setTransient(temp);
            const size_t redundancy = Config::getInstance()->getOption<size_t> (CONFIG_REDUNDANCY);
            arrSchema.setDistribution(defaultPartitioning(redundancy));
            arrSchema.setResidency(_schema.getResidency());
            ArrayID uAId = SystemCatalog::getInstance()->getNextArrayId();
            arrSchema.setIds(uAId, uAId, VersionID(0));
            if (!temp) {
                query->setAutoCommit();
            }
            SystemCatalog::getInstance()->addArray(arrSchema);
        }
        if (temp)                                        // 'temp' flag given?
        {
            syncBarrier(0,query);                        // Workers wait here
            string arrayNameOrg(param<OperatorParamArrayReference>(0)->getObjectName());
            std::string arrayName;
            std::string namespaceName;
            query->getNamespaceArrayNames(arrayNameOrg, namespaceName, arrayName);
            ArrayDesc arrSchema;
            scidb::namespaces::Communicator::getArrayDesc(
                namespaceName, arrayName,SystemCatalog::ANY_VERSION,arrSchema);
            transient::record(make_shared<MemArray>(arrSchema,query));
        }
        return std::shared_ptr<Array>();
    }
};

REGISTER_PHYSICAL_OPERATOR_FACTORY(PhysicalCreateWithResidency, "create_with_residency", "PhysicalCreateWithResidency");

}


