typedef enum {
  NR_SL_SCI_FORMAT_1A = 0;
  NR_SL_SCI_FORMAT_2A = 1;
  NR_SL_SCI_FORMAT_2B = 2;
  NR_SL_SCI_FORMAT_2C = 3;
} sci_format_t;

typedef {
	// 1st stage fields
	uint8_t priority; // 3 bits
	dci_field_t frequency_resource_assignment; // depending on sl-MaxNumPerReserve and N_subChannel^SL 
	dci_field_t time_resource_assignment; // depending on sl_MaxNumPerReserve
	dci_field_t resource_reservation_period; // sl-ResourceReservePeriodList and sl-MultiReserveResource
	dci_field_t dmrs_pattern; // depending on N_pattern and sl-PSSCH-DMRS-TimePatternList
	uint8_t second_stage_sci_format; // 2 bits - Table 8.3.1.1-1
        uint8_t beta_offset_indicator; // 2 bits - depending sl-BetaOffsets2ndSCI and Table 8.3.1.1-2
	uint8_t number_of_dmrs_port; // 1 bit - Table 8.3.1.1-3
	uint8_t mcs; // 5 bits
	dci_field_t additional_mcs; // depending on sl-Additional-MCS-Table
	dci_field_t psfch_overhead; // depending on sl-PSFCH-Period
        dci_field_t reserved; // depending on N_reserved (sl-NumReservedBits) and sl-IndicationUE-B
        dci_field_t conflict_information_receiver; // depending on sl-IndicationUE-B
	// 2nd stage fields
	uint8_t harq_pid; // 4 bits
	uint8_t ndi; // 1 bit
	uint8_t rv_index; // 2 bits
	uint8_t source_id; // 8 bits
	uint16_t dest_id; // 16 bits
	uint8_t harq_feedback; //1 bit
	uint8_t cast_type; // 2 bits formac 2A
	uint8_t csi_req; // 1 bit format 2A, format 2C
	uint16_t zone_id; // 12 bits format 2B
	dci_field_t communication_range; // 4 bits depending on sl-ZoneConfigMCR-Index, format 2B
        uint8_t providing_req_ind; // 1 bit, format 2C
	dci_field_t resource_combinations; // depending on n_subChannel^SL (sl-NumSubchennel), N_rsv_period (sl-ResourceReservePeriodList) and sl-MultiReservedResource, format 2C
        uint8_t first_resource_location; // 8 bits, format 2C
	dci_field_t reference_slot_location; // depending on mu, format 2C
	uint8_t resource_set_type; // 1 bit, format 2C
	dci_field_t lowest_subchannel_indices; // depending on n_subChannel^SL, format 2C
} sci_pdu_t;

