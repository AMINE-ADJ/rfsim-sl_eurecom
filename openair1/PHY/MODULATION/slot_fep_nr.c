/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

#include "PHY/defs_nr_UE.h"
#include "PHY/defs_gNB.h"
#include "modulation_UE.h"
#include "nr_modulation.h"
#include "PHY/LTE_ESTIMATION/lte_estimation.h"
#include "PHY/NR_UE_ESTIMATION/nr_estimation.h"
#include <common/utils/LOG/log.h>

//#define DEBUG_FEP

/*#ifdef LOG_I
#undef LOG_I
#define LOG_I(A,B...) printf(A)
#endif*/

int sl_nr_slot_fep(PHY_VARS_NR_UE *ue,
                UE_nr_rxtx_proc_t *proc,
                unsigned char symbol,
                unsigned char Ns,
                uint32_t sample_offset,
                c16_t rxdataF[][ue->SL_UE_PHY_PARAMS.sl_frame_params.samples_per_slot_wCP])
{
  NR_DL_FRAME_PARMS *frame_params = &ue->SL_UE_PHY_PARAMS.sl_frame_params;
  NR_UE_COMMON *common_vars      = &ue->common_vars;

  AssertFatal(symbol < frame_params->symbols_per_slot, "slot_fep: symbol must be between 0 and %d\n", frame_params->symbols_per_slot-1);
  AssertFatal(Ns < frame_params->slots_per_frame, "slot_fep: Ns must be between 0 and %d\n", frame_params->slots_per_frame-1);

  unsigned int nb_prefix_samples  = frame_params->nb_prefix_samples;
  unsigned int nb_prefix_samples0 = frame_params->nb_prefix_samples0;


  dft_size_idx_t dftsize = get_dft(frame_params->ofdm_symbol_size);
  // This is for misalignment issues
  int32_t tmp_dft_in[8192] __attribute__ ((aligned (32)));

  unsigned int rx_offset = frame_params->get_samples_slot_timestamp(Ns,frame_params,0);
  unsigned int abs_symbol = Ns * frame_params->symbols_per_slot + symbol;

  rx_offset += sample_offset;
  rx_offset += ue->rx_offset;

  for (int idx_symb = Ns*frame_params->symbols_per_slot; idx_symb <= abs_symbol; idx_symb++)
    rx_offset += (idx_symb%(0x7<<frame_params->numerology_index)) ? nb_prefix_samples : nb_prefix_samples0;
  rx_offset += frame_params->ofdm_symbol_size * symbol;

  // use OFDM symbol from within 1/8th of the CP to avoid ISI
  rx_offset -= (nb_prefix_samples / frame_params->ofdm_offset_divisor);

#ifdef SL_DEBUG_SLOT_FEP
  //  if (ue->frame <100)
  LOG_I(PHY, "slot_fep: slot %d, symbol %d, nb_prefix_samples %u, nb_prefix_samples0 %u, rx_offset %u\n",
         Ns, symbol, nb_prefix_samples, nb_prefix_samples0, rx_offset);
#endif

  for (unsigned char aa=0; aa<frame_params->nb_antennas_rx; aa++) {
    memset(&rxdataF[aa][frame_params->ofdm_symbol_size*symbol],0,frame_params->ofdm_symbol_size*sizeof(int32_t));

    int16_t *rxdata_ptr = (int16_t *)&common_vars->rxdata[aa][rx_offset];

    // if input to dft is not 256-bit aligned
    if ((rx_offset & 7) != 0) {
      memcpy((void *)&tmp_dft_in[0],
             (void *)&common_vars->rxdata[aa][rx_offset],
             frame_params->ofdm_symbol_size * sizeof(int32_t));

      rxdata_ptr = (int16_t *)tmp_dft_in;
    }

    dft(dftsize,
        rxdata_ptr,
        (int16_t *)&rxdataF[aa][frame_params->ofdm_symbol_size*symbol],
        1);


    int symb_offset = (Ns%frame_params->slots_per_subframe)*frame_params->symbols_per_slot;
    int32_t rot2 = ((uint32_t*)frame_params->symbol_rotation[1])[symbol+symb_offset];
    ((int16_t*)&rot2)[1]=-((int16_t*)&rot2)[1];

#ifdef SL_DEBUG_SLOT_FEP
    //  if (ue->frame <100)
    LOG_I(PHY, "slot_fep: slot %d, symbol %d rx_offset %u, rotation symbol %d %d.%d\n", Ns,symbol, rx_offset,
	   symbol+symb_offset,((int16_t*)&rot2)[0],((int16_t*)&rot2)[1]);
#endif

    rotate_cpx_vector((c16_t *)&rxdataF[aa][frame_params->ofdm_symbol_size*symbol],
		      (c16_t *)&rot2,
		      (c16_t *)&rxdataF[aa][frame_params->ofdm_symbol_size*symbol],
		      frame_params->ofdm_symbol_size,
		      15);

    int16_t *shift_rot = (int16_t *)frame_params->timeshift_symbol_rotation;

    multadd_cpx_vector((int16_t *)&rxdataF[aa][frame_params->ofdm_symbol_size*symbol],
          shift_rot,
          (int16_t *)&rxdataF[aa][frame_params->ofdm_symbol_size*symbol],
          1,
          frame_params->ofdm_symbol_size,
          15);
  }



  LOG_D(PHY, "SIDELINK RX: Slot FEP: done for symbol:%d\n", symbol);

  return 0;
}

int nr_slot_fep(PHY_VARS_NR_UE *ue,
                NR_DL_FRAME_PARMS *frame_parms,
                UE_nr_rxtx_proc_t *proc,
                unsigned char symbol,
                c16_t rxdataF[][frame_parms->samples_per_slot_wCP],
                uint32_t linktype)
{

  NR_UE_COMMON *common_vars      = &ue->common_vars;
  int Ns = proc->nr_slot_rx;

  AssertFatal(symbol < frame_parms->symbols_per_slot, "slot_fep: symbol must be between 0 and %d\n", frame_parms->symbols_per_slot-1);
  AssertFatal(Ns < frame_parms->slots_per_frame, "slot_fep: Ns must be between 0 and %d\n", frame_parms->slots_per_frame-1);

  unsigned int nb_prefix_samples;
  unsigned int nb_prefix_samples0;
  if (ue->is_synchronized) {
    nb_prefix_samples  = frame_parms->nb_prefix_samples;
    nb_prefix_samples0 = frame_parms->nb_prefix_samples0;
  } else {
    nb_prefix_samples  = frame_parms->nb_prefix_samples;
    nb_prefix_samples0 = frame_parms->nb_prefix_samples;
  }

  dft_size_idx_t dftsize = get_dft(frame_parms->ofdm_symbol_size);
  // This is for misalignment issues
  int32_t tmp_dft_in[8192] __attribute__ ((aligned (32)));

  unsigned int rx_offset = frame_parms->get_samples_slot_timestamp(Ns,frame_parms,0);
  unsigned int abs_symbol = Ns * frame_parms->symbols_per_slot + symbol;
  for (int idx_symb = Ns*frame_parms->symbols_per_slot; idx_symb <= abs_symbol; idx_symb++)
    rx_offset += (idx_symb%(0x7<<frame_parms->numerology_index)) ? nb_prefix_samples : nb_prefix_samples0;
  rx_offset += frame_parms->ofdm_symbol_size * symbol;

  // use OFDM symbol from within 1/8th of the CP to avoid ISI
//  rx_offset -= (nb_prefix_samples / frame_parms->ofdm_offset_divisor);
  rx_offset += 1;

#ifdef DEBUG_FEP
  //  if (ue->frame <100)
  LOG_I(PHY,"slot_fep: slot %d, symbol %d, nb_prefix_samples %u, nb_prefix_samples0 %u, rx_offset %u energy %d\n",
  Ns, symbol, nb_prefix_samples, nb_prefix_samples0, rx_offset, dB_fixed(signal_energy((int32_t *)&common_vars->rxdata[0][rx_offset],frame_parms->ofdm_symbol_size)));
#endif

  for (unsigned char aa=0; aa<frame_parms->nb_antennas_rx; aa++) {
    int16_t *rxdata_ptr = (int16_t *)&common_vars->rxdata[aa][rx_offset];

    // if input to dft is not 256-bit aligned
    if ((rx_offset & 7) != 0) {
      memcpy((void *)&tmp_dft_in[0],
             (void *)&common_vars->rxdata[aa][rx_offset],
             frame_parms->ofdm_symbol_size * sizeof(int32_t));

      rxdata_ptr = (int16_t *)tmp_dft_in;
    }

    start_meas(&ue->rx_dft_stats);

    dft(dftsize,
        rxdata_ptr,
        (int16_t *)&rxdataF[aa][frame_parms->ofdm_symbol_size*symbol],
        1);

    stop_meas(&ue->rx_dft_stats);

/*
    LOG_I(NR_PHY,"%d.%d Applying rotation for symbol %d, linktype %d\n",
          proc->frame_rx,proc->nr_slot_rx,symbol,linktype);
    apply_nr_rotation_RX(frame_parms,
                         rxdataF[aa],
                         frame_parms->symbol_rotation[linktype],
                         Ns,
                         frame_parms->N_RB_DL,
                         0,
                         symbol,
                         linktype);
*/ 
  }

#ifdef DEBUG_FEP
  LOG_I(NR_PHY,"slot_fep: done for Ns %d symbol %d\n",Ns,symbol);
#endif

  return 0;
}

int nr_slot_fep_init_sync(PHY_VARS_NR_UE *ue,
                          UE_nr_rxtx_proc_t *proc,
                          unsigned char symbol,
                          int sample_offset,
                          bool pbch_decoded,
                          c16_t rxdataF[][ue->frame_parms.samples_per_slot_wCP],
                          int link_type)
{
  NR_DL_FRAME_PARMS *frame_parms = &ue->frame_parms;
  NR_UE_COMMON *common_vars   = &ue->common_vars;
  int Ns = proc->nr_slot_rx;

  AssertFatal(symbol < frame_parms->symbols_per_slot, "slot_fep: symbol must be between 0 and %d\n", frame_parms->symbols_per_slot-1);
  AssertFatal(Ns < frame_parms->slots_per_frame, "slot_fep: Ns must be between 0 and %d\n", frame_parms->slots_per_frame-1);

  unsigned int nb_prefix_samples;
  unsigned int nb_prefix_samples0;
  if (pbch_decoded) {
    nb_prefix_samples  = frame_parms->nb_prefix_samples;
    nb_prefix_samples0 = frame_parms->nb_prefix_samples0;
  }
  else {
    nb_prefix_samples  = frame_parms->nb_prefix_samples;
    nb_prefix_samples0 = frame_parms->nb_prefix_samples;
  }
  unsigned int frame_length_samples = frame_parms->samples_per_frame;

  dft_size_idx_t dftsize = get_dft(frame_parms->ofdm_symbol_size);
  // This is for misalignment issues
  int32_t tmp_dft_in[8192] __attribute__ ((aligned (32)));

  unsigned int slot_offset = frame_parms->get_samples_slot_timestamp(Ns,frame_parms,0);
  unsigned int rx_offset   = sample_offset + slot_offset;
  unsigned int abs_symbol  = Ns * frame_parms->symbols_per_slot + symbol;
  for (int idx_symb = Ns*frame_parms->symbols_per_slot; idx_symb <= abs_symbol; idx_symb++)
    rx_offset += (idx_symb%(0x7<<frame_parms->numerology_index)) ? nb_prefix_samples : nb_prefix_samples0;
  rx_offset += frame_parms->ofdm_symbol_size * symbol;

#ifdef DEBUG_FEP
  //  if (ue->frame <100)
  printf("slot_fep: slot %d, symbol %d, nb_prefix_samples %u, nb_prefix_samples0 %u, slot_offset %u, sample_offset %d,rx_offset %u, frame_length_samples %u\n",
         Ns, symbol, nb_prefix_samples, nb_prefix_samples0, slot_offset, sample_offset, rx_offset, frame_length_samples);
#endif

  for (unsigned char aa=0; aa<frame_parms->nb_antennas_rx; aa++) {

    int16_t *rxdata_ptr;
    rx_offset%=frame_length_samples*2;

    if (rx_offset+frame_parms->ofdm_symbol_size > frame_length_samples*2 ) {
      // rxdata is 2 frames len
      // we have to wrap on the end

      memcpy((void *)&tmp_dft_in[0],
             (void *)&common_vars->rxdata[aa][rx_offset],
             (frame_length_samples*2 - rx_offset) * sizeof(int32_t));
      memcpy((void *)&tmp_dft_in[frame_length_samples*2 - rx_offset],
             (void *)&common_vars->rxdata[aa][0],
             (frame_parms->ofdm_symbol_size - (frame_length_samples*2 - rx_offset)) * sizeof(int32_t));
      rxdata_ptr = (int16_t *)tmp_dft_in;

    } else if ((rx_offset & 7) != 0) {

      // if input to dft is not 256-bit aligned
      memcpy((void *)&tmp_dft_in[0],
             (void *)&common_vars->rxdata[aa][rx_offset],
             frame_parms->ofdm_symbol_size * sizeof(int32_t));
      rxdata_ptr = (int16_t *)tmp_dft_in;

    } else {

      // use dft input from RX buffer directly
      rxdata_ptr = (int16_t *)&common_vars->rxdata[aa][rx_offset];

    }

    start_meas(&ue->rx_dft_stats);

    dft(dftsize,
        rxdata_ptr,
        (int16_t *)&rxdataF[aa][frame_parms->ofdm_symbol_size*symbol],
        1);

    stop_meas(&ue->rx_dft_stats);

    int symb_offset = (Ns%frame_parms->slots_per_subframe)*frame_parms->symbols_per_slot;
    c16_t rot2 = frame_parms->symbol_rotation[link_type][symbol + symb_offset];
    rot2.i=-rot2.i;

#ifdef DEBUG_FEP
    //  if (ue->frame <100)
    printf("slot_fep: slot %d, symbol %d rx_offset %u, rotation symbol %d %d.%d\n", Ns,symbol, rx_offset,
	   symbol+symb_offset,rot2.r,rot2.i);
#endif

    c16_t *this_symbol = &rxdataF[aa][frame_parms->ofdm_symbol_size*symbol];
    rotate_cpx_vector(this_symbol, &rot2, this_symbol, frame_parms->ofdm_symbol_size, 15);
  }

#ifdef DEBUG_FEP
  printf("slot_fep: done\n");
#endif

  return 0;
}


int nr_slot_fep_ul(NR_DL_FRAME_PARMS *frame_parms,
                   int32_t *rxdata,
                   int32_t *rxdataF,
                   unsigned char symbol,
                   unsigned char Ns,
                   int sample_offset)
{
  unsigned int nb_prefix_samples  = frame_parms->nb_prefix_samples;
  unsigned int nb_prefix_samples0 = frame_parms->nb_prefix_samples0;

  dft_size_idx_t dftsize = get_dft(frame_parms->ofdm_symbol_size);
  // This is for misalignment issues
  int32_t tmp_dft_in[8192] __attribute__ ((aligned (32)));

  // offset of first OFDM symbol
  unsigned int rxdata_offset = frame_parms->get_samples_slot_timestamp(Ns,frame_parms,0);
  unsigned int abs_symbol = Ns * frame_parms->symbols_per_slot + symbol;
  for (int idx_symb = Ns*frame_parms->symbols_per_slot; idx_symb <= abs_symbol; idx_symb++)
    rxdata_offset += (idx_symb%(0x7<<frame_parms->numerology_index)) ? nb_prefix_samples : nb_prefix_samples0;
  rxdata_offset += frame_parms->ofdm_symbol_size * symbol;

  // use OFDM symbol from within 1/8th of the CP to avoid ISI
  rxdata_offset -= (nb_prefix_samples / frame_parms->ofdm_offset_divisor);

  int16_t *rxdata_ptr;

  if(sample_offset > rxdata_offset) {

    memcpy((void *)&tmp_dft_in[0],
           (void *)&rxdata[frame_parms->samples_per_frame - sample_offset + rxdata_offset],
           (sample_offset - rxdata_offset) * sizeof(int32_t));
    memcpy((void *)&tmp_dft_in[sample_offset - rxdata_offset],
           (void *)&rxdata[0],
           (frame_parms->ofdm_symbol_size - sample_offset + rxdata_offset) * sizeof(int32_t));
    rxdata_ptr = (int16_t *)tmp_dft_in;

  } else if (((rxdata_offset - sample_offset) & 7) != 0) {

    // if input to dft is not 256-bit aligned
    memcpy((void *)&tmp_dft_in[0],
           (void *)&rxdata[rxdata_offset - sample_offset],
           (frame_parms->ofdm_symbol_size) * sizeof(int32_t));
    rxdata_ptr = (int16_t *)tmp_dft_in;

  } else {

    // use dft input from RX buffer directly
    rxdata_ptr = (int16_t *)&rxdata[rxdata_offset - sample_offset];

  }

  dft(dftsize,
      rxdata_ptr,
      (int16_t *)&rxdataF[symbol * frame_parms->ofdm_symbol_size],
      1);

  return 0;
}

void apply_nr_rotation_RX(NR_DL_FRAME_PARMS *frame_parms,
			  c16_t *rxdataF,
                          c16_t *rot,
			  int slot,
                          int nb_rb,
                          int soffset,
			  int first_symbol,
			  int nsymb)
{
  AssertFatal(first_symbol + nsymb <= NR_NUMBER_OF_SYMBOLS_PER_SLOT,
              "First symbol %d and number of symbol %d not compatible with number of symbols in a slot %d\n",
              first_symbol, nsymb, NR_NUMBER_OF_SYMBOLS_PER_SLOT);
  int symb_offset = (slot % frame_parms->slots_per_subframe) * frame_parms->symbols_per_slot;

  for (int symbol = first_symbol; symbol < first_symbol + nsymb; symbol++) {
    
    c16_t rot2 = rot[symbol + symb_offset];
    rot2.i = -rot2.i;
    c16_t *shift_rot = frame_parms->timeshift_symbol_rotation;
    c16_t *this_symbol = &rxdataF[soffset + (frame_parms->ofdm_symbol_size * symbol)];

    if (nb_rb & 1) {
      rotate_cpx_vector(this_symbol, &rot2, this_symbol,
                        (nb_rb + 1) * 6, 15);
      rotate_cpx_vector(this_symbol + frame_parms->first_carrier_offset - 6,
                        &rot2,
                        this_symbol + frame_parms->first_carrier_offset - 6,
                        (nb_rb + 1) * 6, 15);
      multadd_cpx_vector((int16_t *)this_symbol, (int16_t *)shift_rot, (int16_t *)this_symbol,
                         1, (nb_rb + 1) * 6, 15);
      multadd_cpx_vector((int16_t *)(this_symbol + frame_parms->first_carrier_offset - 6),
                         (int16_t *)(shift_rot   + frame_parms->first_carrier_offset - 6),
                         (int16_t *)(this_symbol + frame_parms->first_carrier_offset - 6),
                         1, (nb_rb + 1) * 6, 15);
    } else {
      rotate_cpx_vector(this_symbol, &rot2, this_symbol,
                        nb_rb * 6, 15);
      rotate_cpx_vector(this_symbol + frame_parms->first_carrier_offset,
                        &rot2,
                        this_symbol + frame_parms->first_carrier_offset,
                        nb_rb * 6, 15);
      multadd_cpx_vector((int16_t *)this_symbol, (int16_t *)shift_rot, (int16_t *)this_symbol,
                         1, nb_rb * 6, 15);
      multadd_cpx_vector((int16_t *)(this_symbol + frame_parms->first_carrier_offset),
                         (int16_t *)(shift_rot   + frame_parms->first_carrier_offset),
                         (int16_t *)(this_symbol + frame_parms->first_carrier_offset),
                         1, nb_rb * 6, 15);
    }
  }
}
