/*!
\copyright  Copyright (c) 2015 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       earbud_role.h
\brief	    Define roles for an Earbud.
*/

#ifndef EARBUD_ROLE_H
#define EARBUD_ROLE_H

/*! TWS+ Earbud Roles.
 */
typedef enum
{
    /*! The Earbud will only process the left data in a stereo stream. */
    EARBUD_ROLE_LEFT,
    /*! The Earbud will only process the right data in a stereo stream. */
    EARBUD_ROLE_RIGHT,
    /*! The Earbud will process both the left and right data in a stereo stream. */
    EARBUD_ROLE_STEREO,
    /*! The Earbud will process a mono stream. */
    EARBUD_ROLE_MONO
} earbudRole;

#endif /* EARBUD_ROLE_H */
