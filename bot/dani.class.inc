<?php

/**
 * ranbat aot
 *
 **/

include_once "./conf/dani.conf.inc";

include_once "./bot/base.class.inc";

class tyobot_dani extends tyobot_base {
	var $battles;


	function __construct( &$conn) {
		$this->conn = $conn;
		$battles = Array();
	}

	function _setCommand(&$irc) {
		$irc->registerActionhandler(SMARTIRC_TYPE_CHANNEL, '^'.DANI_ENTRY_CMD.' '	, $this, COMMAND_DANI_ENTRY);
		$irc->registerActionhandler(SMARTIRC_TYPE_CHANNEL, '^'.DANI_ENTRY_CMD_S.' '	, $this, COMMAND_DANI_ENTRY);
		$irc->registerActionhandler(SMARTIRC_TYPE_CHANNEL, '^'.DANI_START_CMD.' '	, $this, COMMAND_DANI_START);
		$irc->registerActionhandler(SMARTIRC_TYPE_CHANNEL, '^'.DANI_START_CMD_S.' '	, $this, COMMAND_DANI_START);
		$irc->registerActionhandler(SMARTIRC_TYPE_CHANNEL, '^'.DANI_WIN_CMD.' '		, $this, COMMAND_DANI_WIN);
		$irc->registerActionhandler(SMARTIRC_TYPE_CHANNEL, '^'.DANI_WIN_CMD_S.' '	, $this, COMMAND_DANI_WIN);
		$irc->registerActionhandler(SMARTIRC_TYPE_CHANNEL, '^'.DANI_CHECK_CMD.' '	, $this, COMMAND_DANI_CHECK);
		$irc->registerActionhandler(SMARTIRC_TYPE_CHANNEL, '^'.DANI_CHECK_CMD_S.' '	, $this, COMMAND_DANI_CHECK);
	}

	// command

	function entry(&$irc, &$data) {
		if (!$this->checkChannel($data)) {
			return;
		}
		$text = $this->_command( $this->_2utf8($data->message), Array( DANI_ENTRY_CMD.' ', DANI_ENTRY_CMD_S.' ' ));
		if( count($text) >= 2 ){
			$char = $this->getCharId( $text[1] );
			if( !$char ){
				$this->_notice($irc, $this->_template( DANI_MSG_ENTRY_ERROR_NO_CHAR, Array('char' => $text[1] ) ), $data->channel );
				return;
			}

			$rs = &$this->conn->Execute( 'select * from entry where ps3id = '.$this->conn->qstr($text[0]).' AND charcter_code = '. $this->conn->qstr($char) );
			if(!$rs){ $this->sendDBError($irc); }
			else{
				if( $rs->RecordCount() ){
					$rec = $rs->FetchRow();
					$cnt = $rec['cnt'];
					$this->_notice($irc, $this->_template( DANI_MSG_ENTRY_ERROR, Array('prof' => $this->getPlayerString($rec) )  ), $data->channel );
				}else{
					$record['ps3id'] = $text[0];
					$record['charcter_code'] = $char;
					$this->conn->AutoExecute( 'entry', $record, 'INSERT');
					if(!$rs){ $this->sendDBError($irc); }
					$rec = $this->conn->GetRow('select * from entry where id = '.$this->conn->qstr( $this->conn->Insert_ID() ));	
					$this->_notice($irc, $this->_template( DANI_MSG_ENTRY_START, Array('prof' => $this->getPlayerString($rec) ) ), $data->channel );
				}
			}   
				
    	}
	}

	function start(&$irc, &$data) {
		if (!$this->checkChannel($data)) {
			return;
		}
		$text = $this->_command( $this->_2utf8($data->message), Array( DANI_START_CMD.' ', DANI_START_CMD_S.' ' ));

		if( count($text) >= 2 ){
			//存在するユーザーかを確認し、存在する場合はレコードを取得する。
			for($i=0;$i<2;$i++){
				if( is_numeric($text[$i]) ){
					$recs[$i] = $this->conn->GetRow('select * from entry where id = '.$this->conn->qstr( $text[$i] ));	
				}else if( mb_substr_count( $text[$i], ':' ) == 1 ){
					$list = explode( ':', $text[$i] );
					$recs[$i] = $this->getEntry( $list[0], $list[1] );
				}else{
					$this->_notice($irc, $this->_template( DANI_ENTRYCODE_ERROR, Array( 'input' => $text[$i] ) ), $data->channel );
					return;
				}
				if( !$recs[$i] ){
					$this->_notice($irc, $this->_template( DANI_MSG_START_ERROR_NO_ENTRY, Array( 'entry' => $text[$i] ) ), $data->channel );
					return;
				}
			}

			if( $recs[0]['ps3id'] == $recs[1]['ps3id'] ){
				$this->_notice($irc, DANI_MSG_START_ERROR_EQ, $data->channel );
				return;
			}
			
			//既に開始されている段位戦に参加していないかどうかを確認する。
			foreach( $this->battles as $battle ){
				for($i=0;$i<2;$i++){
					if( isset($battle['player'][$recs[$i]['id']]) ){
						$this->_notice( $irc, $this->_template( DANI_MSG_START_ERROR_DUP, Array( 'entry' => $this->getPlayerString( $recs[$i] ) ) ) ,$data->channel );
						return;
					}
				}
			}

			//組合せとして段位戦と認定して良いかどうかを判定する
			$ret = $this->checkDaniRule( $recs[0], $recs[1] );
			if( strlen($ret) ){
				$this->_notice( $irc, $ret, $data->channel );
				return;
			}

			//段位戦として登録し、アナウンスを行なう。

			//参戦検索にキーを使うが、勝利結果を受けた際に相手が分かるように値には相手のidを指定する。
			$this->battles[] = Array( 'player' => Array( $recs[0]['id'] => $recs[1]['id'], $recs[1]['id'] => $recs[0]['id'] ) , 'start' => time()  );
			
			
			$option = $this->getAnnounce( $recs );

			$this->_notice( $irc, $this->_template( DANI_MSG_START, Array(
				'profA' => $this->getPlayerString( $recs[0] , true ),
				'profB' => $this->getPlayerString( $recs[1] , true ),
				'option' => $option
				) ), $data->channel );
		}
		
	}
	
	function check(&$irc, &$data) {
		if (!$this->checkChannel($data)) {
			return;
		}
		$text = $this->_command( $this->_2utf8($data->message), Array( DANI_CHECK_CMD.' ', DANI_CHECK_CMD_S.' ' ));
        if( count($text) >= 1 ){
            //存在するユーザーかを確認し、存在する場合はレコードを取得する。
            if( is_numeric($text[0]) ){
                $rec = $this->conn->GetRow('select * from entry where id = '.$this->conn->qstr( $text[0] ) );
            }else if( mb_substr_count( $text[0], ':' ) == 1 ){
                $list = explode( ':', $text[0] );
                $rec = $this->getEntry( $list[0], $list[1] );
            }else{
                $this->_notice($irc, $this->_template( DANI_ENTRYCODE_ERROR, Array( 'input' => $text[0] ) ), $data->channel );
                 return;
            }   
            if( !$rec ){
                $this->_notice($irc, $this->_template( DANI_MSG_START_ERROR_NO_ENTRY, Array( 'entry' => $text[0] ) ), $data->channel );
                return;
            }
			$this->_notice($irc, $this->_template( DANI_MSG_CHECK, Array( 'prof' => $this->getPlayerString( $rec ), 'url' => '' ) ), $data->channel );
		}
	}

	function win(&$irc, &$data) {
		if (!$this->checkChannel($data)) {
			return;
		}
		$text = $this->_command( $this->_2utf8($data->message), Array( DANI_WIN_CMD.' ', DANI_WIN_CMD_S.' ' ));
        if( count($text) >= 1 ){
            //存在するユーザーかを確認し、存在する場合はレコードを取得する。
            if( is_numeric($text[0]) ){
                $rec = $this->conn->GetRow('select * from entry where id = '.$this->conn->qstr( $text[0] ) );
			}else if( mb_substr_count( $text[0], ':' ) == 1 ){
				$list = explode( ':', $text[0] );
                $rec = $this->getEntry( $list[0], $list[1] );
            }else{
                $this->_notice($irc, $this->_template( DANI_ENTRYCODE_ERROR, Array( 'input' => $text[0] ) ), $data->channel );
                 return;
            }   
            if( !$rec ){
                $this->_notice($irc, $this->_template( DANI_MSG_START_ERROR_NO_ENTRY, Array( 'entry' => $text[0] ) ), $data->channel );
                return;
            }   
               
            //既に開始されている段位戦に参加していないかどうかを確認する。
            foreach( $this->battles as $key => $battle ){
                if( isset($battle['player'][$rec['id']] ) ){
					//参加中の段位戦があった為、ここで勝利を処理する。
					$loser = $battle['player'][$rec['id']];
					$lRec = $this->conn->GetRow('select * from entry where id = '.$this->conn->qstr( $loser  ) );

					$this->conn->AutoExecute( 'dani_log', Array(
							'winner' => $rec['id'],
							'rank_w' => $rec['rank'],
							'pt_w' => $rec['pt'],
							'loser' => $loser,
							'rank_l' => $lRec['rank'],
							'pt_l' => $lRec['pt'],
							'start' => $this->battles[$i]['start']
						), 'INSERT');
						
					unset( $this->battles[$key] );

					$option = $this->setDaniRule( $rec, $lRec );
                    $this->_notice( $irc, $this->_template( DANI_MSG_WIN, $rec ). $option ,$data->channel );
                    return;
                }   
            }   
			//参加中の段位戦が無かった。
			$this->_notice( $irc, $this->_template( DANI_MSG_WIN_ERROR, Array( 'prof' => $this->getPlayerString( $rec, true ))), $data->channel  );
		}
	}

	// system

	private function checkChannel(&$data) {
		return (DANI_CHANNEL == $this->_2utf8($data->channel ));
	}

	private function convertRankString( $num ){
		if( $num == 0 ){ return ''; }
		if( $num == 1 ){ return '初段'; }
		$str_array = Array( 1=>'一', 2=>'二', 3=>'三', 4=>'四', 5=>'五', 6=>'六', 7=>'七', 8=>'八', 9=>'九', 0=>'' );
		$ret = "";
		if( $num >= 20 ){
			$ret .= $str_array[ (int)$num/10 ].'十';
			$num = $num % 10;
		}else if( $num >= 10 ){
			$ret .= '十';
			$num = $num % 10;
		}
		$ret .= $str_array[ $num ];

		return $ret."段";
	}

	private function getCharId( $str ){
		if( strlen( $str ) ){
			$str1 = $this->conn->qstr($str);
			$str2 = $this->conn->qstr("%/$str/%");
			$char_code = &$this->conn->GetOne("SELECT code FROM charcter_list WHERE code = $str1 OR name = $str1 OR name_alias like $str2 ");
			if(!$char_code){ return false; }
			else{
                return $char_code;
			}
		}else{
			return false;
		}
	}
	
	private function getCharName( $id ){
		if( strlen( $id ) ){
			$name = &$this->conn->GetOne("SELECT name FROM charcter_list WHERE code = ".$this->conn->qstr( $id ));
			if(!$name){ $this->sendDBError(); }
			else{
				return $name;
			}
		}
		return '';
	}
	
	private function getEntry( $ps3id, $char, $code_f = false ){
		if( !$code_f ){
			$char = $this->getCharId( $char );
		}
		return $this->conn->GetRow( 'select * from entry where ps3id = '.$this->conn->qstr($ps3id).' AND charcter_code = '. $this->conn->qstr($char) );
	}
	
	private function getPlayerString( $player, $s = false ){
		if( !$s ){
			return $this->_template( DANI_TEMPLATE_PROF, Array(
					'eid' => $player['id'],
					'ps3id' => $player['ps3id'],
					'char' => $this->getCharName( $player['charcter_code'] ),
					'rank' => $this->convertRankString( $player['rank']),
					'point' => $player['pt']
				) );
		}else{
			return $this->_template( DANI_TEMPLATE_PROF_S, Array(
					'ps3id' => $player['ps3id'] ,
					'char' => $player['charcter_code'] ,
					'rank' => $this->convertRankString( $player['rank'] ),
					'pt' => $player['pt']) );
		}

		return $this->_template( $base, $player );
	}
	
	private function sendDBError( &$irc = null ){
		d( $this->conn->ErrorMsg() );
		if(is_null($irc)){
			$this->_notice($irc, DANI_DB_ERROR, $data->channel );
		}
	}
	
	private function checkDaniRule( $player1, $player2 ){
		if( abs( $player1['rank'] - $player2['rank'] ) > DANI_CHECK_RANK_MAX_DIFF ){
			return $this->_template( DANI_MSG_START_ERROR_CHECK, Array( 'msg' => DANI_MSG_START_ERROR_MSG_1 ) );
		}
		
		if( ( $player1['pt'] == 2 || $player2['pt'] == 2 ) && abs( $player1['rank'] - $player2['rank'] ) > DANI_CHECK_RANK_UPPER_MAX_DIFF ){
			return $this->_template( DANI_MSG_START_ERROR_CHECK, Array( 'msg' => DANI_MSG_START_ERROR_MSG_2 ) );
		}
		
		$row = $this->conn->GetRow( 'SELECT * FROM dani_log WHERE winner = '. $this->conn->qstr($player1['id']) .' OR loser = '. $this->conn->qstr($player1['id']) .' ORDER BY end desc' );
		if( $row && ( $row['winner'] == $player1['id'] && $row['loser'] == $player2['id'] || $row['winner'] == $player2['id'] && $row['loser'] == $player1['id'] ) ){
			return $this->_template( DANI_MSG_START_ERROR_CHECK, Array( 'msg' => DANI_MSG_START_ERROR_MSG_3  ));	
		}
		$row = $this->conn->GetRow( 'SELECT * FROM dani_log WHERE winner = '. $this->conn->qstr($player1['id']) .' OR loser = '. $this->conn->qstr($player1['id']) .' ORDER     BY end desc' ); 
		if( $row && ( $row['winner'] == $player1['id'] && $row['loser'] == $player2['id'] || $row['winner'] == $player2['id'] && $row['loser'] == $player1['id'] ) ){
		 	return $this->_template( DANI_MSG_START_ERROR_CHECK, Array( 'msg' => DANI_MSG_START_ERROR_MSG_3 ) );
		}
		return "";
	}

	private function setDaniRule( &$winner, &$loser ){
		$ret = "";
		if( ++$winner[ 'pt' ] >= DANI_CHECK_RANK_UP_PT ){
			$winner[ 'rank' ]++;
			$winner[ 'pt' ] = 0; 
			$up = true;
		}
		$this->conn->AutoExecute( 'entry', $winner, 'UPDATE', 'id = '.$winner['id'] );
		
		$ret .= $this->getPlayerString( $winner, true );
		if($up){ $ret .= DANI_MSG_WIN_OPTION_UP; };
		$ret .= DANI_CHAR_SEPARATOR;
		
		$loser['pt']--;
		if( $loser[ 'rank' ] == 1 && $loser[ 'pt' ] < 0 ){ $loser['pt']= 0; }	

		if( $loser[ 'pt' ] <= DANI_CHECK_RANK_DOWN_PT ){
			$loser[ 'rank' ]--;
			$loser[ 'pt' ] = 0;
			$down = true;
		}
		$this->conn->AutoExecute( 'entry', $loser, 'UPDATE', 'id = '.$loser['id'] );

		$ret .= $this->getPlayerString( $loser, true );
		if($down){ $ret .= DANI_MSG_WIN_OPTION_DOWN; };

		$ret .= DANI_MSG_WIN_OPTION_SUFFIX;
		return $ret;
	}

	private function getAnnounce( $players ){
	
		$options = Array();
		foreach( $players as $player ){
			if( $player['pt'] == DANI_CHECK_RANK_UP_PT -1 ){
				$options[] = $this->_template( DANI_ANNOUNCE_UP, Array( 'ps3id' => $player['ps3id'] ) );
			}else if( $player['pt'] == DANI_CHECK_RANK_DOWN_PT +1 ){
				$options[] = $this->_template( DANI_ANNOUNCE_DOWN, Array( 'ps3id' => $player['ps3id'] ) );
			}
		}
		if( count( $options ) ){
			return join(DANI_CHAR_SEPARATOR,$options ).DANI_ANNOUNCE_SUFFIX;
		}
		return "";
	}
}

?>