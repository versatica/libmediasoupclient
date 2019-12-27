const gulp = require('gulp');
const clangFormat = require('gulp-clang-format');

const files =
[
	'src/**/*.cpp',
	'include/**/*.hpp',
	'test/src/**/*.cpp',
	'test/include/**/*.hpp'
];

gulp.task('lint', () =>
{
	const src = files;

	return gulp.src(src)
		.pipe(clangFormat.checkFormat('file', null, { verbose: true, fail: true }));
});

gulp.task('format', () =>
{
	const src = files;

	return gulp.src(src, { base: '.' })
		.pipe(clangFormat.format('file'))
		.pipe(gulp.dest('.'));
});
