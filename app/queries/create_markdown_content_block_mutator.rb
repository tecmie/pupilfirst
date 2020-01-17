class CreateMarkdownContentBlockMutator < ApplicationQuery
  property :target_id, validates: { presence: true }
  property :above_content_block_id

  def create_markdown_content_block
    ContentBlock.transaction do
      markdown_block = ContentBlock.create!(
        block_type: ContentBlock::BLOCK_TYPE_MARKDOWN,
        content: { markdown: "" }
      )

      sort_index = if above_content_block.present?
        # Put at the same position as 'above_content_block'.
        version_of_above_content_block.sort_index
      else
        # Put at the bottom.
        target.content_versions.find_by(version_on: latest_version_date).sort_index + 1
      end

      # Move all content blocks at and below 'above_content_block' one step below.
      ContentVersion.where(version_on: latest_version_date)
        .where('sort_index >= ?', sort_index)
        .update_all('sort_index = sort_index + 1') # rubocop:disable Rails/SkipsModelValidations

      # Now create new content version and slot it into the old position of 'above_content_block'.
      target.content_versions.create!(content_block: markdown_block, version_on: latest_version_date, sort_index: sort_index)
    end
  end

  private

  def version_of_above_content_block
    @version_of_above_content_block ||= above_content_block.content_versions.where(version_on: latest_version_date).first
  end

  def authorized?
    target.present? && (current_school_admin.present? || current_user.course_authors.where(course: target.level.course).exists?)
  end

  def target
    @target ||= Target.find_by(id: target_id)
  end

  def latest_version_date
    @latest_version_date ||= target.latest_content_version_date
  end

  def above_content_block
    @above_content_block ||= begin
      target.content_blocks.find_by(id: above_content_block_id) if above_content_block_id.present?
    end
  end
end