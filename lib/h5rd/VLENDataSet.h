/********************************************************************
 * Copyright © 2017 Computational Molecular Biology Group,          * 
 *                  Freie Universität Berlin (GER)                  *
 *                                                                  *
 * This file is part of ReaDDy.                                     *
 *                                                                  *
 * ReaDDy is free software: you can redistribute it and/or modify   *
 * it under the terms of the GNU Lesser General Public License as   *
 * published by the Free Software Foundation, either version 3 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU Lesser General Public License for more details.              *
 *                                                                  *
 * You should have received a copy of the GNU Lesser General        *
 * Public License along with this program. If not, see              *
 * <http://www.gnu.org/licenses/>.                                  *
 ********************************************************************/


/**
 * << detailed description >>
 *
 * @file VLENDataSet.h
 * @brief << brief description >>
 * @author clonker
 * @date 06.09.17
 * @copyright GNU Lesser General Public License v3.0
 */

#pragma once

#include "Object.h"
#include "DataSetType.h"

namespace h5rd {

class VLENDataSet : public SubObject {
public:
    VLENDataSet(ParentFileRef parentFile, const DataSetType &memoryType, const DataSetType &fileType);

    template<typename T>
    void append(std::vector<std::vector<T>> &data);

    template<typename T>
    void append(const dimensions &dims, std::vector<T> *data);

    ~VLENDataSet() override;

    void close() override;

    void flush();

    std::shared_ptr<DataSpace> getFileSpace() const;

    dimension &extensionDim();

    const dimension &extensionDim() const;

private:
    dimension _extensionDim;
    std::unique_ptr<DataSpace> _memorySpace{nullptr};
    DataSetType _memoryType;
    DataSetType _fileType;
};

}

#include "detail/VLENDataSet_detail.h"