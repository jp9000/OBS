/*

Copyright (c) Microsoft Corporation. All rights reserved.

Module Name:

    winapifamily.h

Abstract:

    Master include file for API family partitioning.

*/

#ifndef _INC_WINAPIFAMILY
#define _INC_WINAPIFAMILY

#if defined(_MSC_VER) && !defined(MOFCOMP_PASS)
#pragma once
#endif // defined(_MSC_VER) && !defined(MOFCOMP_PASS)

/*
 *  Windows APIs can be placed in a partition represented by one of the below bits.   The 
 *  WINAPI_FAMILY value determines which partitions are available to the client code.
 */

#define WINAPI_PARTITION_DESKTOP   0x00000001
#define WINAPI_PARTITION_APP       0x00000002    

/*
 * A family may be defined as the union of multiple families. WINAPI_FAMILY should be set
 * to one of these values.
 */
#define WINAPI_FAMILY_APP          WINAPI_PARTITION_APP
#define WINAPI_FAMILY_DESKTOP_APP  (WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_APP)    

/*
 * A constant that specifies which code is available to the program's target runtime platform.
 * By default we use the 'desktop app' family which places no restrictions on the API surface. 
 * To restrict the API surface to just the App API surface, define WINAPI_FAMILY to WINAPI_FAMILY_APP.
 */
#ifndef WINAPI_FAMILY
#define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP
#endif

/* Macro to determine if a partition is enabled */
#define WINAPI_FAMILY_PARTITION(Partition)	((WINAPI_FAMILY & Partition) == Partition)

/* Macro to determine if only one partition is enabled from a set */
#define WINAPI_FAMILY_ONE_PARTITION(PartitionSet, Partition) ((WINAPI_FAMILY & PartitionSet) == Partition)

/*
 * Macro examples:
 *    The following examples are typical macro usage for enabling/restricting access to header code based
 *    on the target runtime platform. The examples assume a correct setting of the WINAPI_FAMILY macro.
 *
 *      App programs:
 *          Explicitly set WINAPI_FAMILY to WINAPI_PARTITION_APP (cannot access DESKTOP partition)
 *      Desktop programs:
 *          Leave WINAPI_FAMILY set to the default above (currently WINAPI_FAMILY_DESKTOP_APP)
 *
 *      Note: Other families and partitions may be added in the future.
 *
 *
 * The WINAPI_FAMILY_PARTITION macro:
 *    Code-block is available to programs that have access to specified partition.
 *
 *      Example: Available to App and Desktop programs
 *          #if WINAPI_FAMILY_PARTITION( WINAPI_PARTITION_APP )
 *
 *      Example: Available to Desktop programs
 *          #if WINAPI_FAMILY_PARTITION( WINAPI_PARTITION_DESKTOP )
 *
 *
 * The WINAPI_FAMILY_ONE_PARTITION macro:
 *    Code-block is available to programs that have access to specified parition, but not others in the partition set.
 *
 *      Example: Available only to App programs
 *          #if WINAPI_FAMILY_ONE_PARTITION( WINAPI_FAMILY, WINAPI_PARTITION_APP )
 *
 *      Example: Available only to Desktop programs
 *          #if WINAPI_FAMILY_ONE_PARTITION( WINAPI_FAMILY, WINAPI_PARTITION_DESKTOP )
 *
 *      Example: Available to App, but not Desktop programs
 *          #if WINAPI_FAMILY_ONE_PARTITION( WINAPI_FAMILY_DESKTOP_APP, WINAPI_PARTITION_APP )
 */

#endif  /* !_INC_WINAPIFAMILY */
