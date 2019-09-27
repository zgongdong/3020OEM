/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       hdma.h
\brief      Handover Decision Making Algorithm(HDMA) component.
*/

#ifndef HDMA_H_
#define HDMA_H_

#ifdef INCLUDE_SHADOWING 

/*! \brief Initialise HDMA component.
 */
void Hdma_Init(void);

/*! \brief De-Initialise HDMA component.
 */
void Hdma_Destroy(void);

#else

#define Hdma_Init() /* Nothing to do */

#define Hdma_Destroy() /* Nothing to do */

#endif /* INCLUDE_SHADOWING*/


#endif /* HDMA_H_ */
