/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef UB_BUS_INSTANCE_H
#define UB_BUS_INSTANCE_H

#define UBUS_INSTANCE_UNKNOW         (-1)
#define UBUS_INSTANCE_STATIC_SERVER   0
#define UBUS_INSTANCE_STATIC_CLUSTER  1
#define UBUS_INSTANCE_DYNAMIC_SERVER  2
#define UBUS_INSTANCE_DYNAMIC_CLUSTER 3

#define UBUS_INSTANCE_IS_STATIC_SERVER(type) (type == UBUS_INSTANCE_STATIC_SERVER)
#define UBUS_INSTANCE_IS_STATIC_CLUSTER(type) (type == UBUS_INSTANCE_STATIC_CLUSTER)
#define UBUS_INSTANCE_IS_DYNAMIC_SERVER(type) (type == UBUS_INSTANCE_DYNAMIC_SERVER)
#define UBUS_INSTANCE_IS_DYNAMIC_CLUSTER(type) (type == UBUS_INSTANCE_DYNAMIC_CLUSTER)
#define UBUS_INSTANCE_IS_STATIC(type) \
    (UBUS_INSTANCE_IS_STATIC_SERVER(type) || UBUS_INSTANCE_IS_STATIC_CLUSTER(type))
#define UBUS_INSTANCE_IS_DYNAMIC(type) \
    (UBUS_INSTANCE_IS_DYNAMIC_SERVER(type) || UBUS_INSTANCE_IS_DYNAMIC_CLUSTER(type))
#define UBUS_INSTANCE_IS_SERVER(type) \
    (UBUS_INSTANCE_IS_STATIC_SERVER(type) || UBUS_INSTANCE_IS_DYNAMIC_SERVER(type))
#define UBUS_INSTANCE_IS_CLUSTER(type) \
    (UBUS_INSTANCE_IS_STATIC_CLUSTER(type) || UBUS_INSTANCE_IS_DYNAMIC_CLUSTER(type))

#endif
