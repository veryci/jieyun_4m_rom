$('#restart').click(function (e) {
  e.stopPropagation();
  $('#account-c').hide();
  if ($('#restart-c').css('display') === 'none') {
    $('#restart-c').show();
  } else {
    $('#restart-c').hide();
  }
});
$('#account').click(function (e) {
  e.stopPropagation();
  $('#restart-c').hide();
  if ($('#account-c').css('display') === 'none') {
    $('#account-c').show();
  } else {
    $('#account-c').hide();
  }
});
$('#restart-c').click(function (e) {
  e.stopPropagation();
});
$('#account-c').click(function (e) {
  e.stopPropagation();
});
$('.modal').click(function (e) {
  e.stopPropagation();
})
$('body').click(function () {
  $('#restart-c').hide();
  $('#account-c').hide();
});
$('#restart-set').click(function () {
  Alert('是否确定重启？')
  $('.modal-c').hide()
  $('#restart-modal').show();
});
$('#timer-restart-set').click(function () {
  Alert('定时重启路由器')
  $('.modal-c').hide()
  $('#timer-restart-modal').show();
});
$('#shield').click(function () {
  Alert('')
  $('.modal-c').hide()
  $('#shield-done-modal').show();
});
// $('#shield').click(function () {
//   Alert('一键体检')
//   $('.modal-c').hide()
//   $('#shield-modal').show();
// });
$('.btn-cancel').click(function () {
  $('.modal').hide();
});
$('.modal').click(function () {
  $('.modal').hide();
});
$('.alert').click(function (e) {
  e.stopPropagation();
});
$('.close').click(function () {
  $('.modal').hide();
});
$('.header ul.nav li').click(function (e) {
  e.stopPropagation();
  $('.setting > div').hide();
//   $('.header ul.nav li').removeClass('active');
  // $(this).addClass('active');
  if ($(this).attr('id') !== 'higherset') {
    $('#higherset-c').hide();
  }
})
$('#higherset').click(function () {
  if ($('#higherset-c').css('display') === 'none') {
    $('#higherset-c').show();
  } else {
    $('#higherset-c').hide();
    // $(this).removeClass('active');
  }
});
$('#higherset-c').click(function (e) {
  e.stopPropagation();
})
$('body').click(function () {
  $('#higherset-c').hide();
  // $('.header ul.nav li').removeClass('active');
})

$('.setting *').click(function () {
  $('#higherset-c').hide();
  // $('.header ul.nav li').removeClass('active');
})