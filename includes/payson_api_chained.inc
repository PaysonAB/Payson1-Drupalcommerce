<?php
/**
 * 
 * Chained Payments are not generic, for now these are Natureit specific
 * 
 * in case of chained payments with payson, these functions are called. 
 * Chained payment flow
 * 1. Read one or more line items for following, product id and price
 * 2. Read product for each line item for Owner company, email, first and last name, % of payment
 * 3. Create an array of owners from all line item for each company as unique entry, 
 *    We are not supposed to send one company per line item
 * */

/**
 * 
* @param Object $order
 *   The order object the payment request is being made for.
 *
 * @return Array
 *   The response array with owners and orders.
 */
function payson_api_chained($order) {
  // read line items field from order object
  $line_items      = $order->commerce_line_items[LANGUAGE_NONE];
  
  // iterate over each line item to get their id's
  foreach ($line_items AS $item) {
    $line_item_id[] = $item['line_item_id'];
  }
  
  // load line items at once
  $line_item_loaded  = commerce_line_item_load_multiple($line_item_id);

  // $owner will hold all owners data
  $owners = array();
  
  // iterate over each line item and load products
  // get is owner and read Payson information from that owner using payment gateway nid
  foreach ($line_item_loaded as $line_item) {
    $commerce_product_id     = $line_item->commerce_product[LANGUAGE_NONE][0]['product_id'];
    $commerce_product_loaded = commerce_product_load($commerce_product_id);
    $wrapper_items           = entity_metadata_wrapper('commerce_product', $commerce_product_loaded);
    $owner = $wrapper_items->field_product_owner->value();
    
    // we have owner object now, get its payment info
    $gateway_nid = $owner->field_fvof_payment_gateway[LANGUAGE_NONE][0]['nid'];
    $gateway_node = ($gateway_nid)?node_load($gateway_nid):'';
    
    $owner_share = ($gateway_node->field_payment_earning_share[LANGUAGE_NONE][0]['value'])?$gateway_node->field_payment_earning_share[LANGUAGE_NONE][0]['value']:0;
    $static_fee = ($gateway_node->field_payment_static_fee[LANGUAGE_NONE][0]['value'])?$gateway_node->field_payment_static_fee[LANGUAGE_NONE][0]['value']:0;
    $static_fee_limit = ($gateway_node->field_payment_static_fee_limit[LANGUAGE_NONE][0]['value'])?$gateway_node->field_payment_static_fee_limit[LANGUAGE_NONE][0]['value']:39.99;
    
    $owners_info = array(
        'email'         => $gateway_node->field_payment_email[LANGUAGE_NONE][0]['email'],
        'firstName'     => $gateway_node->field_payment_first_name[LANGUAGE_NONE][0]['value'],
        'lastName'      => $gateway_node->field_payment_last_name[LANGUAGE_NONE][0]['value'],
        'price'         => $line_item->commerce_total[LANGUAGE_NONE][0]['amount'],
        'currency_code' => $line_item->commerce_total[LANGUAGE_NONE][0]['currency_code'],
        'primary'       => 'false',
        );
       
    // check if this owner is already in array, if no, add 
    if (array_key_exists($owner->nid, $owners)) {
      // Owner exists, so just update the price
      $owners[$owner->nid]['price'] = $line_item->commerce_total[LANGUAGE_NONE][0]['amount']+$owners[$owner->nid]['price'];
    } else {
      // add owner index and all related info's
      $owners[$owner->nid] = $owners_info;
    }
    
    // Creating an order array
    $amount = $line_item->commerce_unit_price[LANGUAGE_NONE][0]['amount'];
    $currency_code = $line_item->commerce_unit_price[LANGUAGE_NONE][0]['currency_code'];
    //$cost = number_format(commerce_currency_amount_to_decimal($amount, $currency_code), 2, ',', '.');
    //$cost = (float)$cost;
    $cost = paypal_api_price_format($amount, $currency_code);
    
    $orders['orders'][] = new OrderItem(
        $commerce_product_loaded->title,
        $cost,
        $line_item->quantity,
        0,
        $commerce_product_loaded->sku
    );
  
  } // line item

  // iterate each owner and create Payson formatted object
  foreach ($owners as $i => $v) {
    // Owner share is provided when it is chained
    // if a value is present in static fee, that mean this is something extra
    // something extra also requires a level, default would be less than 40 sek
    
    $total_price = paypal_api_price_format($v['price'], $v['currency_code']);
    //$cost = number_format(commerce_currency_amount_to_decimal($v['price'], $v['currency_code']), 2, ',', '.');
    //$cost = (float)$cost*0.90;
    $owner_earning = ($owner_share / 100) * (float)$total_price;
    
    // we have now normal cost, now check if it was a card below, static fees limit, if yes, apply static fees.
    if ($total_price <= $static_fee_limit) {
      $owner_earning = $owner_earning - (float)$static_fee;
    }
    
    //unset extra fields
    unset($owners[$i]['currency_code']);
    
    // array of owners object
    $productOwners['owners'][] = new Receiver(
        $v['email'],
        $owner_earning,
        $v['firstName'],
        $v['lastName'],
        'false'
    );
  } //foreach owner
  
 // drupal_set_message('<pre>' . print_r($static_fee_limit, TRUE) . '</pre>');
  
  //dd($orders, 'Owners');
  return $productOwners+$orders;
}