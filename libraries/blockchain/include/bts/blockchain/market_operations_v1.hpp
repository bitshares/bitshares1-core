struct short_operation_v1
{
     static const operation_type_enum type;
     short_operation_v1():amount(0){}

     asset            get_amount()const { return asset( amount, 0 ); }

     share_type       amount;
     market_index_key short_index;

     void evaluate( transaction_evaluation_state& eval_state )const;
     void evaluate_v1( transaction_evaluation_state& eval_state )const;
};
